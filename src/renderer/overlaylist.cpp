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
};

struct StencilSet
{
};

struct OutlineSet
{
  Color4 color;
};

struct WireframeSet
{
  Color4 color;
};

struct ModelSet
{
  Transform modelworld;
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

  state.assetbarrier = resources->assets()->acquire_barrier();

  bindresource(*commandlist, context.scenedescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// OverlayList::push_gizmo ///////////////////////////
void OverlayList::push_gizmo(OverlayList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material)
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

  bindresource(commandlist, context.gizmopipeline, 0, 0, context.targetwidth, context.targetheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(GizmoSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(GizmoSet));

    auto gizmoset = state.materialset.memory<GizmoSet>(offset);

    gizmoset->color = material->color;
    gizmoset->metalness = material->metalness;
    gizmoset->roughness = material->roughness;
    gizmoset->reflectivity = material->reflectivity;
    gizmoset->emissive = material->emissive;

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
    bindtexture(context.device, state.materialset, ShaderLocation::specularmap, material->specularmap ? material->specularmap->texture : context.whitediffuse);
    bindtexture(context.device, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

#if 1
  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
#else
  push(commandlist, context.pipelinelayout, 0, sizeof(transform), &transform, VK_SHADER_STAGE_VERTEX_BIT);

  draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
#endif
}


///////////////////////// OverlayList::push_wireframe ///////////////////////
void OverlayList::push_wireframe(OverlayList::BuildState &state, lml::Transform const &transform, Mesh const *mesh, Color4 const &color)
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

  bindresource(commandlist, context.wireframepipeline, 0, 0, context.targetwidth, context.targetheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(WireframeSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(WireframeSet));

    auto wireframeset = state.materialset.memory<WireframeSet>(offset);

    wireframeset->color = color;

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

#if 1
  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
#else
  push(commandlist, context.pipelinelayout, 0, sizeof(transform), &transform, VK_SHADER_STAGE_VERTEX_BIT);

  draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
#endif
}


///////////////////////// OverlayList::push_stencil /////////////////////////
void OverlayList::push_stencil(OverlayList::BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, uint32_t reference)
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

  bindresource(commandlist, context.stencilpipeline, 0, 0, context.targetwidth, context.targetheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdSetStencilReference(commandlist, VK_STENCIL_FRONT_AND_BACK, reference);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(StencilSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(StencilSet));

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

#if 1
  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
#else
  push(commandlist, context.pipelinelayout, 0, sizeof(transform), &transform, VK_SHADER_STAGE_VERTEX_BIT);

  draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
#endif
}


///////////////////////// OverlayList::push_outline /////////////////////////
void OverlayList::push_outline(OverlayList::BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, Color4 const &color, uint32_t reference)
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

  bindresource(commandlist, context.outlinepipeline, 0, 0, context.targetwidth, context.targetheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdSetStencilReference(commandlist, VK_STENCIL_FRONT_AND_BACK, reference);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(OutlineSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(OutlineSet));

    auto outlineset = state.materialset.memory<OutlineSet>(offset);

    outlineset->color = color;

    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

#if 1
  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
#else
  push(commandlist, context.pipelinelayout, 0, sizeof(transform), &transform, VK_SHADER_STAGE_VERTEX_BIT);

  draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
#endif
}


///////////////////////// OverlayList::finalise /////////////////////////////
void OverlayList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist->release(state.modelset);
  state.commandlist->release(state.materialset);

  state.commandlist->end();

  state.commandlist = nullptr;

  state.resources->assets()->release_barrier(state.assetbarrier);
}
