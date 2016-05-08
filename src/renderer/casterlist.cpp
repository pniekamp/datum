//
// Datum - caster list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "casterlist.h"
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
  shadowpass = 0,
};

enum ShaderLocation
{
  sceneset = 0,
  materialset = 1,
  modelset = 2,

  albedomap = 1,
};

struct SceneSet
{
  array<Matrix4f, ShadowMap::nslices> shadowview;
};

struct MaterialSet
{
};

struct ModelSet
{
  Transform modelworld;
};


///////////////////////// draw_casters //////////////////////////////////////
void draw_casters(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Casters const &casters)
{
  auto scene = casters.commandlist->lookup<SceneSet>(ShaderLocation::sceneset);

  if (scene)
  {
    scene->shadowview = context.shadows.shadowview;

    execute(commandbuffer, casters.commandlist->commandbuffer());
  }
}


//|---------------------- CasterList ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// CasterList::begin /////////////////////////////////
bool CasterList::begin(BuildState &state, PlatformInterface &platform, RenderContext &context, ResourceManager *resources)
{
  m_commandlist = nullptr;

  state = {};
  state.platform = &platform;
  state.context = &context;
  state.resources = resources;

  if (!context.shadowbuffer)
    return false;

  auto commandlist = resources->allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  if (!commandlist->begin(context.shadowbuffer, context.shadowpass, RenderPasses::shadowpass))
  {
    resources->destroy(commandlist);
    return false;
  }

  state.assetbarrier = resources->assets()->acquire_barrier();

  bindresource(*commandlist, context.shadowpipeline, 0, 0, context.shadows.width, context.shadows.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

  auto sceneset = commandlist->acquire(ShaderLocation::sceneset, context.scenesetlayout, sizeof(SceneSet));

  if (sceneset)
  {
    sceneset.reserve(sizeof(SceneSet));

    bindresource(*commandlist, sceneset, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);

    commandlist->release(sceneset);
  }

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// CasterList::push_material /////////////////////////
void CasterList::push_material(BuildState &state, Material const *material)
{
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

  if (state.material != material)
  {
    state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(MaterialSet), state.materialset);

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(MaterialSet));

      bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

      bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }
}


///////////////////////// CasterList::push_mesh /////////////////////////////
void CasterList::push_mesh(CasterList::BuildState &state, Transform const &transform, Mesh const *mesh)
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

  if (state.mesh != mesh)
  {
    bindresource(commandlist, mesh->vertexbuffer);

    state.mesh = mesh;
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

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// CasterList::push_mesh /////////////////////////////
void CasterList::push_mesh(BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material)
{
  if (state.material != material)
  {
    push_material(state, material);
  }

  push_mesh(state, transform, mesh);
}


///////////////////////// CasterList::finalise //////////////////////////////
void CasterList::finalise(BuildState &state)
{
  assert(state.commandlist);

  auto &commandlist = *state.commandlist;

  commandlist.release(state.modelset);
  commandlist.release(state.materialset);

  state.commandlist->end();

  state.commandlist = nullptr;

  state.resources->assets()->release_barrier(state.assetbarrier);
}
