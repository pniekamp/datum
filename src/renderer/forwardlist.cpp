//
// Datum - forward list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "forwardlist.h"
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
  objectpass = 0,
};

enum ShaderLocation
{
  sceneset = 0,
  materialset = 1,
  modelset = 2,
  computeset = 3,

  albedomap = 1,
  specularmap = 2,
  normalmap = 3,
};

struct ForPlaneSet
{
  Color4 color;
  Vec4 plane;
  float density;
  float startdistance;
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

struct alignas(16) Particle
{
  Vec4 position;
  Matrix2f transform;
  Color4 color;
};

///////////////////////// draw_objects //////////////////////////////////////
void draw_objects(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Objects const &objects)
{
  execute(commandbuffer, objects.commandlist->commandbuffer());
}


//|---------------------- ForwardList ---------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ForwardList::begin ////////////////////////////////
bool ForwardList::begin(BuildState &state, PlatformInterface &platform, RenderContext &context, ResourceManager *resources)
{
  m_commandlist = {};

  state = {};
  state.platform = &platform;
  state.context = &context;
  state.resources = resources;

  if (!context.forwardbuffer)
    return false;

  auto commandlist = resources->allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  if (!commandlist->begin(context.forwardbuffer, context.forwardpass, RenderPasses::objectpass))
  {
    resources->destroy(commandlist);
    return false;
  }

  bindresource(*commandlist, context.scenedescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// ForwardList::push_fogplane ////////////////////////
void ForwardList::push_fogplane(ForwardList::BuildState &state, Color4 const &color, float density, float startdistance, Plane const &plane)
{
  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.fogpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, context.unitquad);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(ForPlaneSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(ForPlaneSet));

    auto materialset = state.materialset.memory<ForPlaneSet>(offset);

    materialset->color = color;
    materialset->plane = Vec4(plane.normal, plane.distance);
    materialset->density = density;
    materialset->startdistance = startdistance * 1.4142135f;

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = Transform::translation(0, 0, startdistance);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, context.unitquad.vertexcount, 1, 0, 0);
  }
}


///////////////////////// ForwardList::push_translucent /////////////////////
void ForwardList::push_translucent(ForwardList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, float alpha)
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

  bindresource(commandlist, context.translucentpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(MaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaterialSet));

    auto materialset = state.materialset.memory<MaterialSet>(offset);

    materialset->color = Color4(material->color, alpha);
    materialset->metalness = material->metalness;
    materialset->roughness = material->roughness;
    materialset->reflectivity = material->reflectivity;
    materialset->emissive = material->emissive;

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


///////////////////////// ForwardList::push_particlesystem //////////////////
void ForwardList::push_particlesystem(ForwardList::BuildState &state, ParticleSystem::Instance const *particles)
{
  if (!particles || particles->count == 0 || particles->spritesheet == nullptr)
    return;

  if (!particles->spritesheet->ready())
  {
    state.resources->request(*state.platform, particles->spritesheet);

    if (!particles->spritesheet->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.particlepipeline[0], 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, context.unitquad);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(MaterialSet), state.materialset);

  if (state.materialset)
  {
    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, particles->spritesheet->texture);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + particles->count*sizeof(Particle))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, particles->count*sizeof(Particle), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(particles->count*sizeof(Particle));

    auto modelset = state.modelset.memory<Particle>(offset);

    for(size_t i = 0; i < particles->count; ++i)
    {
      modelset[i].position = Vec4(particles->position[i], particles->layer[i] - 0.5f + 1e-3);
      modelset[i].transform = particles->transform[i];
      modelset[i].color = particles->color[i];
    }

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, context.unitquad.vertexcount, particles->count, 0, 0);
  }
}


///////////////////////// ForwardList::push_particlesystem //////////////////
void ForwardList::push_particlesystem(ForwardList::BuildState &state, Transform const &transform, ParticleSystem::Instance const *particles)
{
  if (!particles || particles->count == 0 || particles->spritesheet == nullptr)
    return;

  if (!particles->spritesheet->ready())
  {
    state.resources->request(*state.platform, particles->spritesheet);

    if (!particles->spritesheet->ready())
      return;
  }

  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bindresource(commandlist, context.particlepipeline[0], 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(commandlist, context.unitquad);

  state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(MaterialSet), state.materialset);

  if (state.materialset)
  {
    bindtexture(context.device, state.materialset, ShaderLocation::albedomap, particles->spritesheet->texture);

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + particles->count*sizeof(Particle))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, particles->count*sizeof(Particle), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(particles->count*sizeof(Particle));

    auto modelset = state.modelset.memory<Particle>(offset);

    for(size_t i = 0; i < particles->count; ++i)
    {
      modelset[i].position = Vec4(transform * particles->position[i], particles->layer[i] - 0.5f + 1e-3);
      modelset[i].transform = particles->transform[i];
      modelset[i].color = particles->color[i];
    }

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, context.unitquad.vertexcount, particles->count, 0, 0);
  }
}


///////////////////////// ForwardList::finalise /////////////////////////////
void ForwardList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist->release(state.modelset);
  state.commandlist->release(state.materialset);

  state.commandlist->end();

  state.commandlist = nullptr;
}
