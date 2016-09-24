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
  Color4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;
};

struct ModelSet
{
  Transform modelworld;
};


///////////////////////// draw_meshes ///////////////////////////////////////
void draw_meshes(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Meshes const &meshes)
{
  auto scene = meshes.commandlist->lookup<SceneSet>(ShaderLocation::sceneset);

  if (scene)
  {
    scene->worldview = context.proj * context.view;

    execute(commandbuffer, meshes.commandlist->commandbuffer());
  }
}


//|---------------------- MeshList ------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// MeshList::begin ///////////////////////////////////
bool MeshList::begin(BuildState &state, PlatformInterface &platform, RenderContext &context, ResourceManager *resources)
{
  m_commandlist = {};

  state = {};
  state.platform = &platform;
  state.context = &context;
  state.resources = resources;

  if (!context.geometrybuffer)
    return false;

  auto commandlist = resources->allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  if (!commandlist->begin(context.geometrybuffer, context.geometrypass, RenderPasses::geometrypass))
  {
    resources->destroy(commandlist);
    return false;
  }

  state.assetbarrier = resources->assets()->acquire_barrier();

  bindresource(*commandlist, context.geometrypipeline, context.fbox, context.fboy, context.fbowidth - 2*context.fbox, context.fboheight - 2*context.fboy, VK_PIPELINE_BIND_POINT_GRAPHICS);

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
    state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(MaterialSet), state.materialset);

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(MaterialSet));

      auto materialset = state.materialset.memory<MaterialSet>(offset);

      materialset->color = material->color;
      materialset->metalness = material->metalness;
      materialset->roughness = material->roughness;
      materialset->reflectivity = material->reflectivity;
      materialset->emissive = material->emissive;

      bindtexture(context.device, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
      bindtexture(context.device, state.materialset, ShaderLocation::specularmap, material->specularmap ? material->specularmap->texture : context.whitediffuse);
      bindtexture(context.device, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

      bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
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

  if (state.mesh != mesh)
  {
    bindresource(commandlist, mesh->vertexbuffer);

    state.mesh = mesh;
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


///////////////////////// MeshList::push_mesh ///////////////////////////////
void MeshList::push_mesh(BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material)
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

  commandlist.release(state.modelset);
  commandlist.release(state.materialset);

  state.commandlist->end();

  state.commandlist = nullptr;

  state.resources->assets()->release_barrier(state.assetbarrier);
}
