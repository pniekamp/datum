//
// Datum - caster list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "casterlist.h"
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

  passcount
};

enum ShaderLocation
{
  sceneset = 0,
  materialset = 1,
  modelset = 2,

  albedomap = 1,
};

struct MaterialSet
{
};

struct ModelSet
{
  alignas(16) Transform modelworld;
};

struct ActorSet
{
  alignas(16) Transform modelworld;
  alignas(16) Transform bones[1];
};


///////////////////////// draw_casters //////////////////////////////////////
void draw_casters(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Casters const &casters)
{
  execute(commandbuffer, casters.commandlist->commandbuffer(RenderPasses::shadowpass));
}


//|---------------------- CasterList ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// CasterList::begin /////////////////////////////////
bool CasterList::begin(BuildState &state, RenderContext &context, ResourceManager &resources)
{
  m_commandlist = {};

  state = {};
  state.context = &context;
  state.resources = &resources;

  if (!context.prepared)
    return false;

  auto commandlist = resources.allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  if (!commandlist->begin(context.shadowbuffer, context.shadowpass, RenderPasses::passcount))
  {
    resources.destroy(commandlist);
    return false;
  }

  auto shadowpass = commandlist->commandbuffer(RenderPasses::shadowpass);

  bind_descriptor(shadowpass, context.scenedescriptor, context.pipelinelayout, ShaderLocation::sceneset, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// CasterList::push_mesh /////////////////////////////
void CasterList::push_mesh(BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto shadowpass = state.commandlist->commandbuffer(RenderPasses::shadowpass);

  if (state.pipeline != context.modelshadowpipeline)
  {
    bind_pipeline(shadowpass, context.modelshadowpipeline, 0, 0, context.shadows.width, context.shadows.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.modelshadowpipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(shadowpass, 0, mesh->vertexbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(MaterialSet), state.materialset);

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(MaterialSet));

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

      bind_descriptor(shadowpass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
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

    bind_descriptor(shadowpass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(shadowpass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// CasterList::push_mesh /////////////////////////////
void CasterList::push_mesh(BuildState &state, Transform const &transform, Pose const &pose, Mesh const *mesh, Material const *material)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;
  auto shadowpass = state.commandlist->commandbuffer(RenderPasses::shadowpass);

  if (state.pipeline != context.actorshadowpipeline)
  {
    bind_pipeline(shadowpass, context.actorshadowpipeline, 0, 0, context.shadows.width, context.shadows.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.actorshadowpipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(shadowpass, 0, mesh->vertexbuffer);
    bind_vertexbuffer(shadowpass, 1, mesh->rigbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(MaterialSet), state.materialset);

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(MaterialSet));

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

      bind_descriptor(shadowpass, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ActorSet) + pose.bonecount*sizeof(Transform))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ActorSet) + pose.bonecount*sizeof(Transform), state.modelset);
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ActorSet) + pose.bonecount*sizeof(Transform));

    auto modelset = state.modelset.memory<ActorSet>(offset);

    modelset->modelworld = transform;

    copy(pose.bones, pose.bones + pose.bonecount, modelset->bones);

    bind_descriptor(shadowpass, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(shadowpass, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// CasterList::finalise //////////////////////////////
void CasterList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist->release(state.modelset);
  state.commandlist->release(state.materialset);

  state.commandlist->end();

  state.commandlist = nullptr;
}
