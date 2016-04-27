//
// Datum - mesh list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "meshlist.h"
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
  geometrypass = 0,
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

struct SceneSet
{
  Matrix4f worldview;
};

struct MaterialSet
{
  Color4 albedocolor;
  Color3 specularintensity;
  float specularexponent;
};

struct ModelSet
{
  Transform modelworld;
};


///////////////////////// draw_meshes ///////////////////////////////////////
void draw_meshes(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Meshes const &meshes)
{
  SceneSet *scene = (SceneSet*)(context.transfermemory + meshes.meshlist->transferoffset);

  scene->worldview = context.worldview;

  execute(commandbuffer, *meshes.meshlist);
}


//|---------------------- MeshList ------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// MeshList::begin ///////////////////////////////////
bool MeshList::begin(BuildState &state, PlatformInterface &platform, RenderContext &context, ResourceManager *resources)
{
  m_commandlist = nullptr;

  state = {};
  state.platform = &platform;
  state.context = &context;
  state.resources = resources;

  if (!context.gbuffer)
    return false;

  auto commandlist = resources->allocate<CommandList>();

  if (!commandlist)
    return false;

  if (!commandlist->begin(context, context.gbuffer, context.geometrypass, RenderPasses::geometrypass, sizeof(SceneSet)))
  {
    resources->destroy(commandlist);
    return false;
  }

  state.assetbarrier = resources->assets()->acquire_barrier();

  bindresource(*commandlist, context.geometrypipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(*commandlist, context.sceneset, context.pipelinelayout, ShaderLocation::sceneset, commandlist->transferoffset, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// MeshList::push_material ///////////////////////////
void MeshList::push_material(BuildState &state, Material const *material)
{
  if (!material)
    return;

  if (!material->ready())
  {
    state.material = nullptr;

    state.resources->request(*state.platform, material);

    if (!material->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  if (state.material != material)
  {
    if (state.materialset)
      commandlist.release(state.materialset, state.materialoffset);

    state.materialoffset = 0;
    state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(MaterialSet));

    if (state.materialset)
    {
      bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
      bindtexture(context.device, state.materialset, ShaderLocation::specularmap, material->specularmap ? material->specularmap->texture : context.whitediffuse);
      bindtexture(context.device, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

      auto materialset = state.materialset.memory<MaterialSet>(state.materialoffset);

      materialset->albedocolor = material->albedocolor;
      materialset->specularintensity = material->specularintensity;
      materialset->specularexponent = material->specularexponent;

      bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, state.materialoffset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.materialoffset = alignto(state.materialoffset + sizeof(MaterialSet), state.materialset.alignment());
    }

    state.material = material;
  }
}


///////////////////////// MeshList::push_mesh ///////////////////////////////
void MeshList::push_mesh(MeshList::BuildState &state, Transform const &transform, Mesh const *mesh)
{
  if (!mesh)
    return;

  if (!mesh->ready())
  {
    state.resources->request(*state.platform, mesh);

    if (!mesh->ready())
      return;
  }

  if (!state.material)
    return;

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  if (state.modelset.capacity() < state.modeloffset + sizeof(ModelSet))
  {
    if (state.modelset)
      commandlist.release(state.modelset, state.modeloffset);

    state.modeloffset = 0;
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet));
  }

  if (state.modeloffset + sizeof(ModelSet) <= state.modelset.capacity())
  {
    auto modelset = state.modelset.memory<ModelSet>(state.modeloffset);

    modelset->modelworld = transform;

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, state.modeloffset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    if (state.mesh != mesh)
    {
      bindresource(commandlist, mesh->vertexbuffer);

      state.mesh = mesh;
    }

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);

    state.modeloffset = alignto(state.modeloffset + sizeof(ModelSet), state.modelset.alignment());
  }
}


///////////////////////// MeshList::push_mesh ///////////////////////////////
void MeshList::push_mesh(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material)
{
  if (state.material != material)
  {
    push_material(state, material);
  }

  push_mesh(state, transform, mesh);
}


///////////////////////// MeshList::finalise ////////////////////////////////
void MeshList::finalise(BuildState &state)
{
  assert(state.commandlist);

  auto &commandlist = *state.commandlist;

  if (state.modelset)
    commandlist.release(state.modelset, state.modeloffset);

  if (state.materialset)
    commandlist.release(state.materialset, state.materialoffset);

  state.commandlist->end();

  state.commandlist = nullptr;

  state.resources->assets()->release_barrier(state.assetbarrier);
}
