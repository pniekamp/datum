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

enum ShaderLocation
{
  sceneset = 0,
  materialset = 1,
  modelset = 2,
  extendedset = 3,

  albedomap = 1,
  surfacemap = 2,
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
};

struct LineMaterialSet
{
  alignas(16) Color4 color;
  alignas(16) Vec4 texcoords;
  alignas( 4) float depthfade;
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

struct GizmoModelSet
{
  alignas(16) Transform modelworld;
  alignas(16) Vec3 size;
};

struct MaskModelSet
{
  alignas(16) Transform modelworld;
};

struct FillModelSet
{
  alignas(16) Transform modelworld;
};

struct PathModelSet
{
  alignas(16) Transform modelworld;
  alignas( 4) float halfwidth;
  alignas( 4) float overhang;
};

struct LineModelSet
{
  alignas(16) Transform modelworld;
  alignas(16) Vec3 size;
  alignas( 4) float halfwidth;
  alignas( 4) float overhang;
};

struct OutlineModelSet
{
  alignas(16) Transform modelworld;
};

struct WireframeModelSet
{
  alignas(16) Transform modelworld;
};

///////////////////////// draw_overlays /////////////////////////////////////
void draw_overlays(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Overlays const &overlays)
{
  execute(commandbuffer, overlays.overlaycommands);
}


//|---------------------- OverlayList ---------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// OverlayList::begin ////////////////////////////////
bool OverlayList::begin(BuildState &state, RenderContext &context, ResourceManager &resources)
{
  m_commandlump = {};

  state = {};
  state.context = &context;
  state.resources = &resources;
  state.clipx = 0;
  state.clipy = 0;
  state.clipwidth = context.width;
  state.clipheight = context.height;

  if (!context.ready)
    return false;

  auto commandlump = resources.allocate<CommandLump>(&context);

  if (!commandlump)
    return false;

  overlaycommands = commandlump->allocate_commandbuffer();

  if (!overlaycommands)
  {
    resources.destroy(commandlump);
    return false;
  }

  using Vulkan::begin;

  begin(context.vulkan, overlaycommands, context.framebuffer, context.overlaypass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::sceneset, context.scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlump = { resources, commandlump };

  state.commandlump = commandlump;

  return true;
}


///////////////////////// OverlayList::push_gizmo ///////////////////////////
void OverlayList::push_gizmo(OverlayList::BuildState &state, Vec3 const &position, Vec3 const &size, Quaternion3f const &rotation, Mesh const *mesh, Material const *material, Color4 const &tint)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.gizmopipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(overlaycommands, 0, mesh->vertexbuffer);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(GizmoMaterialSet), std::move(state.materialset));

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
    bind_texture(context.vulkan, state.materialset, ShaderLocation::surfacemap, material->surfacemap ? material->surfacemap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(GizmoModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(GizmoModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(GizmoModelSet));

    auto modelset = state.modelset.memory<GizmoModelSet>(offset);

    modelset->modelworld = Transform::translation(position) * Transform::rotation(rotation);
    modelset->size = size;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_wireframe ///////////////////////
void OverlayList::push_wireframe(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Color4 const &color)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.wireframepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(overlaycommands, 0, mesh->vertexbuffer);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(WireframeMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(WireframeMaterialSet));

    auto materialset = state.materialset.memory<WireframeMaterialSet>(offset);

    materialset->color = color;
    materialset->depthfade = state.depthfade;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(WireframeModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(WireframeModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(WireframeModelSet));

    auto modelset = state.modelset.memory<WireframeModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilmask /////////////////////
void OverlayList::push_stencilmask(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, uint32_t reference)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.stencilmaskpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaycommands, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaycommands, 0, mesh->vertexbuffer);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaskMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaskMaterialSet));

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(MaskModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(MaskModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(MaskModelSet));

    auto modelset = state.modelset.memory<MaskModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilmask /////////////////////
void OverlayList::push_stencilmask(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, uint32_t reference)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.stencilmaskpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaycommands, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaycommands, 0, mesh->vertexbuffer);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaskMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaskMaterialSet));

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(MaskModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(MaskModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(MaskModelSet));

    auto modelset = state.modelset.memory<MaskModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilfill /////////////////////
void OverlayList::push_stencilfill(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Color4 const &color, uint32_t reference)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.stencilfillpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaycommands, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaycommands, 0, mesh->vertexbuffer);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(FillMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(FillMaterialSet));

    auto materialset = state.materialset.memory<FillMaterialSet>(offset);

    materialset->color = color;
    materialset->texcoords = Vec4(0, 0, 1, 1);
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(FillModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(FillModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(FillModelSet));

    auto modelset = state.modelset.memory<FillModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilfill /////////////////////
void OverlayList::push_stencilfill(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, Vec2 const &base, Vec2 const &tiling, uint32_t reference)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.stencilfillpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaycommands, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaycommands, 0, mesh->vertexbuffer);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(FillMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(FillMaterialSet));

    auto materialset = state.materialset.memory<FillMaterialSet>(offset);

    materialset->color = material->color;
    materialset->texcoords = Vec4(base.x, base.y, tiling.x, tiling.y);
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(FillModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(FillModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(FillModelSet));

    auto modelset = state.modelset.memory<FillModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilpath /////////////////////
void OverlayList::push_stencilpath(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Color4 const &color, float thickness, uint32_t reference)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.stencilpathpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaycommands, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaycommands, 0, mesh->vertexbuffer);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(PathMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(PathMaterialSet));

    auto materialset = state.materialset.memory<PathMaterialSet>(offset);

    materialset->color = color;
    materialset->texcoords = Vec4(0, 0, 1, 1);
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(PathModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(PathModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(PathModelSet));

    auto modelset = state.modelset.memory<PathModelSet>(offset);

    modelset->modelworld = transform;
    modelset->halfwidth = 2*thickness;
    modelset->overhang = thickness;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilpath /////////////////////
void OverlayList::push_stencilpath(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, Vec2 const &base, Vec2 const &tiling, float thickness, uint32_t reference)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.stencilpathpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  set_stencil_reference(overlaycommands, VK_STENCIL_FRONT_AND_BACK, reference);

  bind_vertexbuffer(overlaycommands, 0, mesh->vertexbuffer);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(PathMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(PathMaterialSet));

    auto materialset = state.materialset.memory<PathMaterialSet>(offset);

    materialset->color = material->color;
    materialset->texcoords = Vec4(base.x, base.y, tiling.x, tiling.y);
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(PathModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(PathModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(PathModelSet));

    auto modelset = state.modelset.memory<PathModelSet>(offset);

    modelset->modelworld = transform;
    modelset->halfwidth = 2*thickness;
    modelset->overhang = thickness;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_line ////////////////////////////
void OverlayList::push_line(OverlayList::BuildState &state, Vec3 const &a, Vec3 const &b, Color4 const &color, float thickness)
{
  assert(state.commandlump);

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.linepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(overlaycommands, 0, context.unitquad);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(LineMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(LineMaterialSet));

    auto materialset = state.materialset.memory<LineMaterialSet>(offset);

    materialset->color = color;
    materialset->texcoords = Vec4(0, 0, 1, 1);
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(LineModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(LineModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(LineModelSet));

    auto modelset = state.modelset.memory<LineModelSet>(offset);

    modelset->modelworld = Transform::translation((a + b)/2) * Transform::rotation(Vec3(0, 0, 1), theta(b - a)) * Transform::rotation(Vec3(0, 1, 0), phi(b - a) - pi<float>()/2) * Transform::rotation(Vec3(0, 0, 1), -pi<float>()/2);
    modelset->size = Vec3(0, norm(b - a)/2, 0);
    modelset->halfwidth = thickness + 2.0f;
    modelset->overhang = 0.0f;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, 2, 1, 0, 0);
  }
}


///////////////////////// OverlayList::push_volume //////////////////////////
void OverlayList::push_lines(BuildState &state, Vec3 const &position, Vec3 const &size, Quaternion3f const &rotation, Mesh const *mesh, Color4 const &color, float thickness)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.linepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(overlaycommands, 0, mesh->vertexbuffer);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(LineMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(LineMaterialSet));

    auto materialset = state.materialset.memory<LineMaterialSet>(offset);

    materialset->color = color;
    materialset->texcoords = Vec4(0, 0, 1, 1);
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(LineModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(LineModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(LineModelSet));

    auto modelset = state.modelset.memory<LineModelSet>(offset);

    modelset->modelworld = Transform::translation(position) * Transform::rotation(rotation);
    modelset->size = size;
    modelset->halfwidth = thickness + 2.0f;
    modelset->overhang = 0.0f;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
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
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  bind_pipeline(overlaycommands, context.outlinepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(overlaycommands, 0, mesh->vertexbuffer);

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(OutlineMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(OutlineMaterialSet));

    auto materialset = state.materialset.memory<OutlineMaterialSet>(offset);

    materialset->color = color;
    materialset->depthfade = state.depthfade;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.available() < sizeof(OutlineModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(OutlineModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(OutlineModelSet));

    auto modelset = state.modelset.memory<OutlineModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(overlaycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(overlaycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
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
  assert(state.commandlump);

  auto &context = *state.context;

  end(context.vulkan, overlaycommands);

  state.commandlump = nullptr;
}
