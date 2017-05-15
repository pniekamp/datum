//
// Datum - forward list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "forwardlist.h"
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

struct Environment
{
  alignas(16) Vec3 halfdim;
  alignas(16) Transform invtransform;
};

struct TranslucentMaterialSet
{
  alignas(16) Color4 color;
  alignas( 4) float roughness;
  alignas( 4) float reflectivity;
  alignas( 4) float emissive;
};

struct ForPlaneMaterialSet
{
  alignas(16) Vec4 plane;
  alignas(16) Color4 color;
  alignas( 4) float density;
  alignas( 4) float falloff;
  alignas( 4) float startdistance;
};

struct ParticleMaterialSet
{
};

struct Particle
{
  alignas(16) Vec4 position;
  alignas(16) Matrix2f transform;
  alignas(16) Color4 color;
};

struct WaterMaterialSet
{
  alignas(16) Color4 color;
  alignas( 4) float metalness;
  alignas( 4) float roughness;
  alignas( 4) float reflectivity;
  alignas( 4) float emissive;
  alignas(16) Vec3 bumpscale;
  alignas( 8) Vec2 flow;
  alignas(16) Environment specular;
};

struct ModelSet
{
  alignas(16) Transform modelworld;
};

///////////////////////// draw_objects //////////////////////////////////////
void draw_objects(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Objects const &objects)
{
  execute(commandbuffer, objects.commandlist->commandbuffer());
}


//|---------------------- ForwardList ---------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ForwardList::begin ////////////////////////////////
bool ForwardList::begin(BuildState &state, RenderContext &context, ResourceManager *resources)
{
  m_commandlist = {};

  state = {};
  state.context = &context;
  state.resources = resources;

  if (!context.prepared)
    return false;

  auto commandlist = resources->allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  if (!commandlist->begin(context.forwardbuffer, context.forwardpass, RenderPasses::objectpass))
  {
    resources->destroy(commandlist);
    return false;
  }

  bind_descriptor(*commandlist, context.scenedescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// ForwardList::push_fogplane ////////////////////////
void ForwardList::push_fogplane(ForwardList::BuildState &state, Color4 const &color, Plane const &plane, float density, float startdistance, float falloff)
{
  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bind_pipeline(commandlist, context.fogpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(commandlist, 0, context.unitquad);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(ForPlaneMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(ForPlaneMaterialSet));

    auto materialset = state.materialset.memory<ForPlaneMaterialSet>(offset);

    materialset->plane = Vec4(plane.normal, plane.distance);
    materialset->color = color;
    materialset->density = density;
    materialset->falloff = falloff;
    materialset->startdistance = max(startdistance, 0.0f) * 1.4142135f;

    bind_descriptor(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = Transform::translation(0, 0, max(startdistance, 0.01f));

    bind_descriptor(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, context.unitquad.vertexcount, 1, 0, 0);
  }
}


///////////////////////// ForwardList::push_translucent /////////////////////
void ForwardList::push_translucent(ForwardList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, float alpha)
{ 
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bind_pipeline(commandlist, context.translucentpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(commandlist, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(TranslucentMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(TranslucentMaterialSet));

    auto materialset = state.materialset.memory<TranslucentMaterialSet>(offset);

    materialset->color = Color4(material->color.rgb, material->color.a * alpha);
    materialset->roughness = material->roughness;
    materialset->reflectivity = material->reflectivity;
    materialset->emissive = material->emissive;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::specularmap, material->specularmap ? material->specularmap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

    bind_descriptor(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// ForwardList::push_particlesystem //////////////////
void ForwardList::push_particlesystem(ForwardList::BuildState &state, ParticleSystem const *particlesystem, ParticleSystem::Instance const *particles)
{
  assert(state.commandlist);
  assert(particlesystem && particlesystem->ready());
  assert(particles);

  if (particles->count == 0)
    return;

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bind_pipeline(commandlist, context.particlepipeline[0], 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(commandlist, 0, context.unitquad);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(ParticleMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(ParticleMaterialSet));

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, particlesystem->spritesheet->texture);

    bind_descriptor(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + particles->count*sizeof(Particle))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, particles->count*sizeof(Particle), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(particles->count*sizeof(Particle));

    auto modelset = state.modelset.memory<Particle>(offset);

    for(int i = 0; i < particles->count; ++i)
    {
      modelset[i].position = Vec4(particles->position[i], particles->layer[i] - 0.5f + 1e-3f);
      modelset[i].transform = particles->transform[i];
      modelset[i].color = particles->color[i];
    }

    bind_descriptor(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, context.unitquad.vertexcount, particles->count, 0, 0);
  }
}


///////////////////////// ForwardList::push_particlesystem //////////////////
void ForwardList::push_particlesystem(ForwardList::BuildState &state, Transform const &transform, ParticleSystem const *particlesystem, ParticleSystem::Instance const *particles)
{ 
  assert(state.commandlist);
  assert(particlesystem && particlesystem->ready());
  assert(particles);

  if (particles->count == 0)
    return;

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bind_pipeline(commandlist, context.particlepipeline[0], 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(commandlist, 0, context.unitquad);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(ParticleMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(ParticleMaterialSet));

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, particlesystem->spritesheet->texture);

    bind_descriptor(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + particles->count*sizeof(Particle))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, particles->count*sizeof(Particle), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(particles->count*sizeof(Particle));

    auto modelset = state.modelset.memory<Particle>(offset);

    for(int i = 0; i < particles->count; ++i)
    {
      modelset[i].position = Vec4(transform * particles->position[i], particles->layer[i] - 0.5f + 1e-3f);
      modelset[i].transform = particles->transform[i];
      modelset[i].color = particles->color[i];
    }

    bind_descriptor(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, context.unitquad.vertexcount, particles->count, 0, 0);
  }
}


///////////////////////// ForwardList::push_water ///////////////////////////
void ForwardList::push_water(ForwardList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, EnvMap const *envmap, Vec2 const &flow, Vec3 const &bumpscale, float alpha)
{
  auto envtransform = Transform::identity();
  auto envdimension = Vec3(2e5f, 2e5f, 2e5f);

  push_water(state, transform, mesh, material, envtransform, envdimension, envmap, flow, bumpscale, alpha);
}


///////////////////////// ForwardList::push_water ///////////////////////////
void ForwardList::push_water(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, Transform const &envtransform, SkyBox const *skybox, Vec2 const &flow, Vec3 const &bumpscale, float alpha)
{
  assert(skybox && skybox->ready());

  auto envdimension = Vec3(2e5f, 2e5f, 2e5f);

  push_water(state, transform, mesh, material, envtransform, envdimension, skybox, flow, bumpscale, alpha);
}


///////////////////////// ForwardList::push_water ///////////////////////////
void ForwardList::push_water(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, Transform const &envtransform, Vec3 const &envdimension, EnvMap const *envmap, Vec2 const &flow, Vec3 const &bumpscale, float alpha)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());
  assert(envmap && envmap->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  bind_pipeline(commandlist, context.waterpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(commandlist, 0, mesh->vertexbuffer);

  state.materialset = commandlist.acquire(context.materialsetlayout, sizeof(WaterMaterialSet), state.materialset);

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(WaterMaterialSet));

    auto materialset = state.materialset.memory<WaterMaterialSet>(offset);

    materialset->color = Color4(material->color.rgb, material->color.a * alpha);
    materialset->metalness = material->metalness;
    materialset->roughness = material->roughness;
    materialset->reflectivity = material->reflectivity;
    materialset->emissive = material->emissive;
    materialset->bumpscale = Vec3(bumpscale.xy, 1.0f / (0.01f + bumpscale.z));
    materialset->flow = flow;
    materialset->specular.halfdim = envdimension/2;
    materialset->specular.invtransform = inverse(envtransform);

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::specularmap, envmap->texture);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

    bind_descriptor(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
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
