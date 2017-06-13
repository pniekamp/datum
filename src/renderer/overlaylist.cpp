//
// Datum - overlay list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "overlaylist.h"
#include <leap/lml/matrix.h>
#include <leap/lml/matrixconstants.h>
#include "debug.h"

using namespace std;
using namespace lml;
using namespace Vulkan;
using namespace DatumPlatform;
using leap::alignto;
using leap::extentof;

enum RenderPasses
{
  overlaypass = 0,

  passcount
};

enum ShaderLocation
{
  sceneset = 0,
  materialset = 1,
  modelset = 2,

  albedomap = 1,
  specularmap = 2,
  normalmap = 3,
};

struct GizmoMaterialSet
{
  alignas(16) Color4 color;
  alignas( 4) float metalness;
  alignas( 4) float roughness;
  alignas( 4) float reflectivity;
  alignas( 4) float emissive;
  alignas( 4) float depthfade;
};

struct MaskMaterialSet
{
};

struct FillMaterialSet
{
  alignas(16) Color4 color;
  alignas(16) Vec4 texcoords;
  alignas( 4) float depthfade;
};

struct PathMaterialSet
{
  alignas(16) Color4 color;
  alignas(16) Vec4 texcoords;
  alignas( 4) float depthfade;
  alignas( 4) float halfwidth;
  alignas( 4) float overhang;
};

struct OutlineMaterialSet
{
  alignas(16) Color4 color;
  alignas( 4) float depthfade;
};

struct WireframeMaterialSet
{
  alignas(16) Color4 color;
  alignas( 4) float depthfade;
};

struct ModelSet
{
  alignas(16) Transform modelworld;
  alignas(16) Vec4 size;
};


///////////////////////// draw_overlays /////////////////////////////////////
void draw_overlays(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Overlays const &overlays)
{ 
  execute(commandbuffer, overlays.commandlist->commandbuffer(RenderPasses::overlaypass));
}


//|---------------------- OverlayList ---------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// OverlayList::begin ////////////////////////////////
bool OverlayList::begin(BuildState &state, RenderContext &context, ResourceManager &resources)
{
  m_commandlist = {};

  state = {};
  state.context = &context;
  state.resources = &resources;
  state.clipx = 0;
  state.clipy = 0;
  state.clipwidth = context.width;
  state.clipheight = context.height;

  if (!context.prepared)
    return false;

  auto commandlist = resources.allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  if (!commandlist->begin(context.framebuffer, context.renderpass, RenderPasses::passcount))
  {
    resources.destroy(commandlist);
    return false;
  }

  auto overlaypass = commandlist->commandbuffer(RenderPasses::overlaypass);

  bind_descriptor(overlaypass, context.scenedescriptor, context.pipelinelayout, ShaderLocation::sceneset, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// OverlayList::push_gizmo ///////////////////////////
void OverlayList::push_gizmo(OverlayList::BuildState &state, Vec3 const &position, Vec3 const &size, Quaternion3f const &rotation, Mesh const *mesh, Material const *material, Color4 const &tint)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.gizmopipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(overlaypass, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(GizmoMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(GizmoMaterialSet));

    auto materialset = state.materialset.memory<GizmoMaterialSet>(offset);

    materialset->color = hada(material->color, tint);
    materialset->metalness = material->metalness;
    materialset->roughness = material->roughness;
    materialset->reflectivity = material->reflectivity;
    materialset->emissive = material->emissive;
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::specularmap, material->specularmap ? material->specularmap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = Transform::translation(position) * Transform::rotation(rotation);
    modelset->size = Vec4(size, 1);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_wireframe ///////////////////////
void OverlayList::push_wireframe(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Color4 const &color)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.wireframepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(overlaypass, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(WireframeMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(WireframeMaterialSet));

    auto materialset = state.materialset.memory<WireframeMaterialSet>(offset);

    materialset->color = color;
    materialset->depthfade = state.depthfade;

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilmask /////////////////////
void OverlayList::push_stencilmask(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, uint32_t reference)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.stencilmaskpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaypass, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaypass, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(MaskMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaskMaterialSet));

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilmask /////////////////////
void OverlayList::push_stencilmask(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, uint32_t reference)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.stencilmaskpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaypass, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaypass, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(MaskMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaskMaterialSet));

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilfill /////////////////////
void OverlayList::push_stencilfill(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Color4 const &color, uint32_t reference)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.stencilfillpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaypass, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaypass, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(FillMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(FillMaterialSet));

    auto materialset = state.materialset.memory<FillMaterialSet>(offset);

    materialset->color = color;
    materialset->texcoords = Vec4(0, 0, 1, 1);
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilfill /////////////////////
void OverlayList::push_stencilfill(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, Vec2 const &base, Vec2 const &tiling, uint32_t reference)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.stencilfillpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaypass, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaypass, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(FillMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(FillMaterialSet));

    auto materialset = state.materialset.memory<FillMaterialSet>(offset);

    materialset->color = material->color;
    materialset->texcoords = Vec4(base.x, base.y, tiling.x, tiling.y);
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilpath /////////////////////
void OverlayList::push_stencilpath(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Color4 const &color, float thickness, uint32_t reference)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.stencilpathpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaypass, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaypass, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(PathMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(PathMaterialSet));

    auto materialset = state.materialset.memory<PathMaterialSet>(offset);

    materialset->color = color;
    materialset->texcoords = Vec4(0, 0, 1, 1);
    materialset->halfwidth = 2*thickness;
    materialset->overhang = thickness;
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilpath /////////////////////
void OverlayList::push_stencilpath(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, Vec2 const &base, Vec2 const &tiling, float thickness, uint32_t reference)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.stencilpathpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaypass, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaypass, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(PathMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(PathMaterialSet));

    auto materialset = state.materialset.memory<PathMaterialSet>(offset);

    materialset->color = material->color;
    materialset->texcoords = Vec4(base.x, base.y, tiling.x, tiling.y);
    materialset->halfwidth = 2*thickness;
    materialset->overhang = thickness;
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_line ////////////////////////////
void OverlayList::push_line(OverlayList::BuildState &state, Vec3 const &a, Vec3 const &b, Color4 const &color, float thickness)
{
  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.linepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(overlaypass, 0, context.unitquad);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(PathMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(PathMaterialSet));

    auto materialset = state.materialset.memory<PathMaterialSet>(offset);

    materialset->color = color;
    materialset->texcoords = Vec4(0, 0, 1, 1);
    materialset->halfwidth = thickness + 2.0f;
    materialset->overhang = 0.0f;
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = Transform::translation((a + b)/2) * Transform::rotation(Vec3(0, 0, 1), theta(b - a)) * Transform::rotation(Vec3(0, 1, 0), phi(b - a) - pi<float>()/2) * Transform::rotation(Vec3(0, 0, 1), -pi<float>()/2);
    modelset->size = Vec4(Vec3(0, norm(b - a)/2, 0), 0);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, 2, 1, 0, 0);
  }
}


///////////////////////// OverlayList::push_volume //////////////////////////
void OverlayList::push_lines(BuildState &state, Vec3 const &position, Vec3 const &size, Quaternion3f const &rotation, Mesh const *mesh, Color4 const &color, float thickness)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.linepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(overlaypass, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(PathMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(PathMaterialSet));

    auto materialset = state.materialset.memory<PathMaterialSet>(offset);

    materialset->color = color;
    materialset->texcoords = Vec4(0, 0, 1, 1);
    materialset->halfwidth = thickness + 2.0f;
    materialset->overhang = 0.0f;
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = Transform::translation(position) * Transform::rotation(rotation);
    modelset->size = Vec4(size, 1);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_volume //////////////////////////
void OverlayList::push_volume(BuildState &state, Bound3 const &bound, Mesh const *mesh, Color4 const &color, float thickness)
{
  push_lines(state, bound.centre(), bound.halfdim(), Quaternion3f(1.0f, 0.0f, 0.0f, 0.0f), mesh, color, thickness);
}


///////////////////////// OverlayList::push_outline /////////////////////////
void OverlayList::push_outline(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, Color4 const &color)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto overlaypass = commandlist.commandbuffer(RenderPasses::overlaypass);

  bind_pipeline(overlaypass, context.outlinepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(overlaypass, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(OutlineMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(OutlineMaterialSet));

    auto materialset = state.materialset.memory<OutlineMaterialSet>(offset);

    materialset->color = color;
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bind_descriptor(overlaypass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bind_descriptor(overlaypass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaypass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_scissor //////////////////////////
void OverlayList::push_scissor(BuildState &state, Rect2 const &cliprect)
{
  state.clipx = (int)(cliprect.min.x);
  state.clipy = (int)(cliprect.min.y);
  state.clipwidth = (int)(cliprect.max.x - cliprect.min.x);
  state.clipheight = (int)(cliprect.max.y - cliprect.min.y);
}


///////////////////////// OverlayList::finalise /////////////////////////////
void OverlayList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist->release(state.modelset);
  state.commandlist->release(state.materialset);

  state.commandlist->end();

  state.commandlist = nullptr;
}
