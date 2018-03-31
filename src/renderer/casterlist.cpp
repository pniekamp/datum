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

enum ShaderLocation
{
  sceneset = 0,
  materialset = 1,
  modelset = 2,
  extendedset = 3,

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

struct FoilageSet
{
  alignas(16) Vec4 wind;
  alignas(16) Vec3 bendscale;
  alignas(16) Vec3 detailbendscale;
  alignas(16) Transform modelworlds[1];
};

///////////////////////// draw_casters //////////////////////////////////////
void draw_casters(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Casters const &casters)
{
  execute(commandbuffer, casters.castercommands);
}


//|---------------------- CasterList ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// CasterList::begin /////////////////////////////////
bool CasterList::begin(BuildState &state, RenderContext &context, ResourceManager &resources)
{
  m_commandlump = {};

  state = {};
  state.context = &context;
  state.resources = &resources;

  if (!context.ready)
    return false;

  auto commandlump = resources.allocate<CommandLump>(&context);

  if (!commandlump)
    return false;

  castercommands = commandlump->allocate_commandbuffer();

  if (!castercommands)
  {
    resources.destroy(commandlump);
    return false;
  }

  using Vulkan::begin;

  begin(context.vulkan, castercommands, context.shadowbuffer, context.shadowpass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::sceneset, context.scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlump = { resources, commandlump };

  state.commandlump = commandlump;

  return true;
}


///////////////////////// CasterList::push_mesh /////////////////////////////
void CasterList::push_mesh(BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.pipeline != context.modelshadowpipeline)
  {
    bind_pipeline(castercommands, context.modelshadowpipeline, 0, 0, context.shadows.width, context.shadows.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.modelshadowpipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(castercommands, 0, mesh->vertexbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(MaterialSet));

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

      bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  if (state.modelset.available() < sizeof(ModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(ModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(castercommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// CasterList::push_mesh /////////////////////////////
void CasterList::push_mesh(BuildState &state, Transform const &transform, Pose const &pose, Mesh const *mesh, Material const *material)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.pipeline != context.actorshadowpipeline)
  {
    bind_pipeline(castercommands, context.actorshadowpipeline, 0, 0, context.shadows.width, context.shadows.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.actorshadowpipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(castercommands, 0, mesh->vertexbuffer);
    bind_vertexbuffer(castercommands, 1, mesh->rigbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(MaterialSet));

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

      bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  size_t actorsetsize = sizeof(ActorSet) + (pose.bonecount-1)*sizeof(Transform);

  if (state.modelset.available() < actorsetsize)
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, actorsetsize, std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(actorsetsize);

    auto modelset = state.modelset.memory<ActorSet>(offset);

    modelset->modelworld = transform;

    copy(pose.bones, pose.bones + pose.bonecount, modelset->bones);

    bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(castercommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// CasterList::push_foilage //////////////////////////
void CasterList::push_foilage(BuildState &state, Transform const *transforms, size_t count, Mesh const *mesh, Material const *material, Vec4 const &wind, Vec3 const &bendscale, Vec3 const &detailbendscale)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.pipeline != context.foilageshadowpipeline)
  {
    bind_pipeline(castercommands, context.foilageshadowpipeline, 0, 0, context.shadows.width, context.shadows.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.foilageshadowpipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(castercommands, 0, mesh->vertexbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(MaterialSet));

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

      bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  size_t foilagesetsize = sizeof(FoilageSet) + (count-1)*sizeof(Transform);

  if (state.modelset.available() < foilagesetsize)
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, foilagesetsize, std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(foilagesetsize);

    auto modelset = state.modelset.memory<FoilageSet>(offset);

    modelset->wind = wind;
    modelset->bendscale = bendscale;
    modelset->detailbendscale = detailbendscale;

    for(size_t i = 0; i < count; ++i)
    {
      modelset->modelworlds[i] = transforms[i];
    }

    bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(castercommands, mesh->vertexbuffer.indexcount, count, 0, 0, 0);
  }
}


///////////////////////// CasterList::finalise //////////////////////////////
void CasterList::finalise(BuildState &state)
{
  assert(state.commandlump);

  auto &context = *state.context;

  end(context.vulkan, castercommands);

  state.commandlump = nullptr;
}
