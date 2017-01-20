//
// Datum - overlay list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "overlaylist.h"
#include "renderer.h"
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

struct GizmoSet
{
  Color4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;
  float depthfade;
};

struct MaskSet
{
};

struct FillSet
{
  Color4 color;
  Vec4 texcoords;
  float depthfade;
};

struct PathSet
{
  Color4 color;
  Vec4 texcoords;
  float depthfade;
  float halfwidth;
  float overhang;
};

struct OutlineSet
{
  Color4 color;
  float depthfade;
};

struct WireframeSet
{
  Color4 color;
  float depthfade;
};

struct ModelSet
{
  Transform modelworld;
  Vec4 size;
};


///////////////////////// draw_overlays /////////////////////////////////////
void draw_overlays(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Overlays const &overlays)
{ 
  execute(commandbuffer, overlays.commandlist->commandbuffer());
}


//|---------------------- OverlayList ---------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// OverlayList::begin ////////////////////////////////
bool OverlayList::begin(BuildState &state, PlatformInterface &platform, RenderContext &context, ResourceManager *resources)
{
  m_commandlist = {};

  state = {};
  state.platform = &platform;
  state.context = &context;
  state.resources = resources;
  state.clipx = 0;
  state.clipy = 0;
  state.clipwidth = context.width;
  state.clipheight = context.height;

  if (!context.framebuffer)
    return false;

  auto commandlist = resources->allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  if (!commandlist->begin(context.framebuffer, context.renderpass, RenderPasses::overlaypass))
  {
    resources->destroy(commandlist);
    return false;
  }

  bindresource(*commandlist, context.scenedescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// OverlayList::push_gizmo ///////////////////////////
void OverlayList::push_gizmo(OverlayList::BuildState &state, Vec3 const &position, Vec3 const &size, Quaternion3f const &rotation, Mesh const *mesh, Material const *material, Color4 const &tint)
{
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  if (!material)
    return;

  if (!material->ready())
  {
    state.resources->request(*state.platform, material);

    if (!material->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.gizmopipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(GizmoSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(GizmoSet));

    auto gizmoset = state.materialset.memory<GizmoSet>(offset);

    gizmoset->color = hada(Color4(material->color, 1.0f), tint);
    gizmoset->metalness = material->metalness;
    gizmoset->roughness = material->roughness;
    gizmoset->reflectivity = material->reflectivity;
    gizmoset->emissive = material->emissive;
    gizmoset->depthfade = state.depthfade;

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
    bindtexture(context.device, state.materialset, ShaderLocation::specularmap, material->specularmap ? material->specularmap->texture : context.whitediffuse);
    bindtexture(context.device, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = Transform::translation(position) * Transform::rotation(rotation);
    modelset->size = Vec4(size, 1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_wireframe ///////////////////////
void OverlayList::push_wireframe(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Color4 const &color)
{
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.wireframepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(WireframeSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(WireframeSet));

    auto wireframeset = state.materialset.memory<WireframeSet>(offset);

    wireframeset->color = color;
    wireframeset->depthfade = state.depthfade;

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilmask /////////////////////
void OverlayList::push_stencilmask(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, uint32_t reference)
{
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.stencilmaskpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdSetStencilReference(commandlist, VK_STENCIL_FRONT_AND_BACK, reference);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(MaskSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaskSet));

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilmask /////////////////////
void OverlayList::push_stencilmask(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, uint32_t reference)
{
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  if (!material)
    return;

  if (!material->ready())
  {
    state.resources->request(*state.platform, material);

    if (!material->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.stencilmaskpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdSetStencilReference(commandlist, VK_STENCIL_FRONT_AND_BACK, reference);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(MaskSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaskSet));

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilfill /////////////////////
void OverlayList::push_stencilfill(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Color4 const &color, uint32_t reference)
{
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.stencilfillpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdSetStencilReference(commandlist, VK_STENCIL_FRONT_AND_BACK, reference);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(FillSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(FillSet));

    auto fillset = state.materialset.memory<FillSet>(offset);

    fillset->color = color;
    fillset->texcoords = Vec4(0, 0, 1, 1);
    fillset->depthfade = state.depthfade;

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilfill /////////////////////
void OverlayList::push_stencilfill(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, Vec2 const &base, Vec2 const &tiling, uint32_t reference)
{
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  if (!material)
    return;

  if (!material->ready())
  {
    state.resources->request(*state.platform, material);

    if (!material->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.stencilfillpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdSetStencilReference(commandlist, VK_STENCIL_FRONT_AND_BACK, reference);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(FillSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(FillSet));

    auto fillset = state.materialset.memory<FillSet>(offset);

    fillset->color = material->color;
    fillset->texcoords = Vec4(base.x, base.y, tiling.x, tiling.y);
    fillset->depthfade = state.depthfade;

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilpath /////////////////////
void OverlayList::push_stencilpath(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Color4 const &color, float thickness, uint32_t reference)
{
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.stencilpathpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdSetStencilReference(commandlist, VK_STENCIL_FRONT_AND_BACK, reference);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(PathSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(PathSet));

    auto pathset = state.materialset.memory<PathSet>(offset);

    pathset->color = color;
    pathset->texcoords = Vec4(0, 0, 1, 1);
    pathset->halfwidth = 2*thickness;
    pathset->overhang = thickness;
    pathset->depthfade = state.depthfade;

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_stencilpath /////////////////////
void OverlayList::push_stencilpath(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, Vec2 const &base, Vec2 const &tiling, float thickness, uint32_t reference)
{
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  if (!material)
    return;

  if (!material->ready())
  {
    state.resources->request(*state.platform, material);

    if (!material->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.stencilpathpipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdSetStencilReference(commandlist, VK_STENCIL_FRONT_AND_BACK, reference);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(PathSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(PathSet));

    auto pathset = state.materialset.memory<PathSet>(offset);

    pathset->color = material->color;
    pathset->texcoords = Vec4(base.x, base.y, tiling.x, tiling.y);
    pathset->halfwidth = 2*thickness;
    pathset->overhang = thickness;
    pathset->depthfade = state.depthfade;

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_line ////////////////////////////
void OverlayList::push_line(OverlayList::BuildState &state, Vec3 const &a, Vec3 const &b, Color4 const &color, float thickness)
{
  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.linepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, context.unitquad);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(PathSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(PathSet));

    auto pathset = state.materialset.memory<PathSet>(offset);

    pathset->color = color;
    pathset->texcoords = Vec4(0, 0, 1, 1);
    pathset->halfwidth = thickness + 2.0f;
    pathset->overhang = 0.0f;
    pathset->depthfade = state.depthfade;

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = Transform::translation((a + b)/2) * Transform::rotation(Vec3(0, 0, 1), theta(b - a)) * Transform::rotation(Vec3(0, 1, 0), phi(b - a) - pi<float>()/2) * Transform::rotation(Vec3(0, 0, 1), -pi<float>()/2);
    modelset->size = Vec4(Vec3(0, norm(b - a)/2, 0), 0);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, 2, 1, 0, 0);
  }
}


///////////////////////// OverlayList::push_volume //////////////////////////
void OverlayList::push_lines(BuildState &state, Vec3 const &position, Vec3 const &size, Quaternion3f const &rotation, Mesh const *mesh, Color4 const &color, float thickness)
{
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.linepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(PathSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(PathSet));

    auto pathset = state.materialset.memory<PathSet>(offset);

    pathset->color = color;
    pathset->texcoords = Vec4(0, 0, 1, 1);
    pathset->halfwidth = thickness + 2.0f;
    pathset->overhang = 0.0f;
    pathset->depthfade = state.depthfade;

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = Transform::translation(position) * Transform::rotation(rotation);
    modelset->size = Vec4(size, 1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
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
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  if (!material)
    return;

  if (!material->ready())
  {
    state.resources->request(*state.platform, material);

    if (!material->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.outlinepipeline, 0, 0, context.width, context.height, state.clipx, state.clipy, state.clipwidth, state.clipheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(OutlineSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(OutlineSet));

    auto outlineset = state.materialset.memory<OutlineSet>(offset);

    outlineset->color = color;
    outlineset->depthfade = state.depthfade;

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;
    modelset->size = Vec4(1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// OverlayList::push_scissor //////////////////////////
void OverlayList::push_scissor(BuildState &state, Rect2 const &cliprect)
{
  state.clipx = cliprect.min.x;
  state.clipy = cliprect.min.y;
  state.clipwidth = cliprect.max.x - cliprect.min.x;
  state.clipheight = cliprect.max.y - cliprect.min.y;
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
