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
using leap::alignto;
using leap::extentof;

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

struct MaterialSet
{
  alignas(16) Color4 color;
  alignas( 4) float metalness;
  alignas( 4) float roughness;
  alignas( 4) float reflectivity;
  alignas( 4) float emissive;
};

struct FogPlaneMaterialSet
{
  alignas(16) Vec4 plane;
  alignas(16) Color4 color;
  alignas( 4) float density;
  alignas( 4) float falloff;
  alignas( 4) float startdistance;
};

struct Particle
{
  alignas(16) Vec4 position;
  alignas(16) Matrix2f transform;
  alignas(16) Color4 color;
};

struct ParticleMaterialSet
{
};

struct Environment
{
  alignas(16) Vec3 halfdim;
  alignas(16) Transform invtransform;
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

struct ParticleSet
{
  alignas(16) Particle particles[1];
};

///////////////////////// commands //////////////////////////////////////////

auto bind_pipeline_command(Vulkan::Pipeline const &pipeline)
{
  Renderable::Forward::Command command = {};
  command.type = Renderable::Forward::Command::Type::bind_pipeline;
  command.bind_pipeline.pipeline = &pipeline;

  return command;
}

auto bind_vertexbuffer_command(uint32_t binding, Vulkan::VertexBuffer const &vertexbuffer)
{
  Renderable::Forward::Command command = {};
  command.type = Renderable::Forward::Command::Type::bind_vertexbuffer;
  command.bind_vertexbuffer.binding = binding;
  command.bind_vertexbuffer.vertexbuffer = &vertexbuffer;

  return command;
}

auto bind_descriptor_command(uint32_t set, VkDescriptorSet descriptor, VkDeviceSize offset)
{
  Renderable::Forward::Command command = {};
  command.type = Renderable::Forward::Command::Type::bind_descriptor;
  command.bind_descriptor.set = set;
  command.bind_descriptor.descriptor = descriptor;
  command.bind_descriptor.offset = offset;

  return command;
}

auto draw_command(uint32_t vertexcount, uint32_t instancecount)
{
  Renderable::Forward::Command command = {};
  command.type = Renderable::Forward::Command::Type::draw;
  command.draw.vertexcount = vertexcount;
  command.draw.instancecount = instancecount;

  return command;
}

auto draw_indexed_command(uint32_t indexcount, uint32_t instancecount)
{
  Renderable::Forward::Command command = {};
  command.type = Renderable::Forward::Command::Type::draw_indexed;
  command.draw_indexed.indexcount = indexcount;
  command.draw_indexed.instancecount = instancecount;

  return command;
}


///////////////////////// push_command //////////////////////////////////////
void push_command(ForwardList::BuildState &state, Renderable::Forward::Command **&commands, Renderable::Forward::Command const &command)
{
  auto &commandlump = *state.commandlump;

  if (state.commandset.available() < sizeof(Renderable::Forward::Command))
  {
    state.commandset = commandlump.acquire_descriptor(sizeof(Renderable::Forward::Command), std::move(state.commandset));
  }

  if (state.commandset.capacity() != 0)
  {
    auto offset = state.commandset.reserve(sizeof(Renderable::Forward::Command));

    *commands = state.commandset.memory<Renderable::Forward::Command>(offset);

    **commands = command;

    commands = &(*commands)->next;
  }
}


///////////////////////// draw_forward //////////////////////////////////////
void draw_forward(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Forward::Command const *commands)
{
  for(Renderable::Forward::Command const *command = commands; command; command = command->next)
  {
    switch (command->type)
    {
      case Renderable::Forward::Command::Type::bind_pipeline:
        bind_pipeline(commandbuffer, *command->bind_pipeline.pipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);
        break;

      case Renderable::Forward::Command::Type::bind_vertexbuffer:
        bind_vertexbuffer(commandbuffer, command->bind_vertexbuffer.binding, *command->bind_vertexbuffer.vertexbuffer);
        break;

      case Renderable::Forward::Command::Type::bind_descriptor:
        bind_descriptor(commandbuffer, context.pipelinelayout, command->bind_descriptor.set, command->bind_descriptor.descriptor, command->bind_descriptor.offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
        break;

      case Renderable::Forward::Command::Type::draw:
        draw(commandbuffer, command->draw.vertexcount, command->draw.instancecount, 0, 0);
        break;

      case Renderable::Forward::Command::Type::draw_indexed:
        draw(commandbuffer, command->draw_indexed.indexcount, command->draw_indexed.instancecount, 0, 0, 0);
        break;

      default:
        assert(false);
    }
  }
}


//|---------------------- ForwardList ---------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ForwardList::begin ////////////////////////////////
bool ForwardList::begin(BuildState &state, RenderContext &context, ResourceManager &resources)
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

  solidcommands = nullptr;
  blendcommands = nullptr;
  colorcommands = nullptr;

  state.solidcommand = &solidcommands;
  state.blendcommand = &blendcommands;
  state.colorcommand = &colorcommands;

  m_commandlump = { resources, commandlump };

  state.commandlump = commandlump;

  return true;
}


///////////////////////// ForwardList::push_opaque //////////////////////////
void ForwardList::push_opaque(ForwardList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  push_command(state, state.solidcommand, bind_pipeline_command(context.opaquepipeline));

  push_command(state, state.solidcommand, bind_vertexbuffer_command(0, mesh->vertexbuffer));

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaterialSet));

    auto materialset = state.materialset.memory<MaterialSet>(offset);

    materialset->color = material->color;
    materialset->metalness = material->metalness;
    materialset->roughness = material->roughness;
    materialset->reflectivity = material->reflectivity;
    materialset->emissive = material->emissive;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::surfacemap, material->surfacemap ? material->surfacemap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

    push_command(state, state.solidcommand, bind_descriptor_command(ShaderLocation::materialset, state.materialset, offset));
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

    push_command(state, state.solidcommand, bind_descriptor_command(ShaderLocation::modelset, state.modelset, offset));

    push_command(state, state.solidcommand, draw_indexed_command(mesh->vertexbuffer.indexcount, 1));
  }
}


///////////////////////// ForwardList::push_translucent /////////////////////
void ForwardList::push_translucent(ForwardList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, float alpha)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  push_command(state, state.colorcommand, bind_pipeline_command(context.translucentpipeline));

  push_command(state, state.colorcommand, bind_vertexbuffer_command(0, mesh->vertexbuffer));

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaterialSet));

    auto materialset = state.materialset.memory<MaterialSet>(offset);

    materialset->color = Color4(material->color.rgb, material->color.a * alpha);
    materialset->metalness = material->metalness;
    materialset->roughness = material->roughness;
    materialset->reflectivity = material->reflectivity;
    materialset->emissive = material->emissive;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::surfacemap, material->surfacemap ? material->surfacemap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

    push_command(state, state.colorcommand, bind_descriptor_command(ShaderLocation::materialset, state.materialset, offset));
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

    push_command(state, state.colorcommand, bind_descriptor_command(ShaderLocation::modelset, state.modelset, offset));

    push_command(state, state.colorcommand, draw_indexed_command(mesh->vertexbuffer.indexcount, 1));
  }
}


///////////////////////// ForwardList::push_translucent /////////////////////
void ForwardList::push_translucent_wb(ForwardList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, float alpha)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  push_command(state, state.blendcommand, bind_pipeline_command(context.translucentblendpipeline));

  push_command(state, state.blendcommand, bind_vertexbuffer_command(0, mesh->vertexbuffer));

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaterialSet));

    auto materialset = state.materialset.memory<MaterialSet>(offset);

    materialset->color = Color4(material->color.rgb, material->color.a * alpha);
    materialset->metalness = material->metalness;
    materialset->roughness = material->roughness;
    materialset->reflectivity = material->reflectivity;
    materialset->emissive = material->emissive;

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::surfacemap, material->surfacemap ? material->surfacemap->texture : context.whitediffuse);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

    push_command(state, state.blendcommand, bind_descriptor_command(ShaderLocation::materialset, state.materialset, offset));
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

    push_command(state, state.blendcommand, bind_descriptor_command(ShaderLocation::modelset, state.modelset, offset));

    push_command(state, state.blendcommand, draw_indexed_command(mesh->vertexbuffer.indexcount, 1));
  }
}


///////////////////////// ForwardList::push_particlesystem //////////////////
void ForwardList::push_particlesystem(ForwardList::BuildState &state, ParticleSystem const *particlesystem, ParticleSystem::Instance const *particles)
{
  assert(state.commandlump);
  assert(particlesystem && particlesystem->ready());
  assert(particles);

  if (particles->count == 0)
    return;

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  push_command(state, state.colorcommand, bind_pipeline_command(context.particlepipeline));

  push_command(state, state.colorcommand, bind_vertexbuffer_command(0, context.unitquad));

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(ParticleMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(ParticleMaterialSet));

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, particlesystem->spritesheet->texture);

    push_command(state, state.colorcommand, bind_descriptor_command(ShaderLocation::materialset, state.materialset, offset));
  }

  size_t particlesetsize = particles->count * sizeof(Particle);

  if (state.modelset.available() < particlesetsize)
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, particlesetsize, std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(particlesetsize);

    auto modelset = state.modelset.memory<Particle>(offset);

    for(int i = 0; i < particles->count; ++i)
    {
      modelset[i].position = Vec4(particles->position[i], particles->layer[i] - 0.5f + 1e-3f);
      modelset[i].transform = particles->transform[i];
      modelset[i].color = particles->color[i];
    }

    push_command(state, state.colorcommand, bind_descriptor_command(ShaderLocation::modelset, state.modelset, offset));

    push_command(state, state.colorcommand, draw_command(context.unitquad.vertexcount, particles->count));
  }
}


///////////////////////// ForwardList::push_particlesystem //////////////////
void ForwardList::push_particlesystem(ForwardList::BuildState &state, Transform const &transform, ParticleSystem const *particlesystem, ParticleSystem::Instance const *particles)
{
  assert(state.commandlump);
  assert(particlesystem && particlesystem->ready());
  assert(particles);

  if (particles->count == 0)
    return;

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  push_command(state, state.colorcommand, bind_pipeline_command(context.particlepipeline));

  push_command(state, state.colorcommand, bind_vertexbuffer_command(0, context.unitquad));

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(ParticleMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(ParticleMaterialSet));

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, particlesystem->spritesheet->texture);

    push_command(state, state.colorcommand, bind_descriptor_command(ShaderLocation::materialset, state.materialset, offset));
  }

  if (state.modelset.available() < particles->count*sizeof(Particle))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, particles->count*sizeof(Particle), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(particles->count*sizeof(Particle));

    auto modelset = state.modelset.memory<Particle>(offset);

    for(int i = 0; i < particles->count; ++i)
    {
      modelset[i].position = Vec4(transform * particles->position[i], particles->layer[i] - 0.5f + 1e-3f);
      modelset[i].transform = particles->transform[i];
      modelset[i].color = particles->color[i];
    }

    push_command(state, state.colorcommand, bind_descriptor_command(ShaderLocation::modelset, state.modelset, offset));

    push_command(state, state.colorcommand, draw_command(context.unitquad.vertexcount, particles->count));
  }
}


///////////////////////// ForwardList::push_particlesystem //////////////////
void ForwardList::push_particlesystem_wb(ForwardList::BuildState &state, ParticleSystem const *particlesystem, ParticleSystem::Instance const *particles)
{
  assert(state.commandlump);
  assert(particlesystem && particlesystem->ready());
  assert(particles);

  if (particles->count == 0)
    return;

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  push_command(state, state.blendcommand, bind_pipeline_command(context.particleblendpipeline));

  push_command(state, state.blendcommand, bind_vertexbuffer_command(0, context.unitquad));

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(ParticleMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(ParticleMaterialSet));

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, particlesystem->spritesheet->texture);

    push_command(state, state.blendcommand, bind_descriptor_command(ShaderLocation::materialset, state.materialset, offset));
  }

  size_t particlesetsize = particles->count * sizeof(Particle);

  if (state.modelset.available() < particlesetsize)
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, particlesetsize, std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(particlesetsize);

    auto modelset = state.modelset.memory<Particle>(offset);

    for(int i = 0; i < particles->count; ++i)
    {
      modelset[i].position = Vec4(particles->position[i], particles->layer[i] - 0.5f + 1e-3f);
      modelset[i].transform = particles->transform[i];
      modelset[i].color = particles->color[i];
    }

    push_command(state, state.blendcommand, bind_descriptor_command(ShaderLocation::modelset, state.modelset, offset));

    push_command(state, state.blendcommand, draw_command(context.unitquad.vertexcount, particles->count));
  }
}


///////////////////////// ForwardList::push_particlesystem //////////////////
void ForwardList::push_particlesystem_wb(ForwardList::BuildState &state, Transform const &transform, ParticleSystem const *particlesystem, ParticleSystem::Instance const *particles)
{
  assert(state.commandlump);
  assert(particlesystem && particlesystem->ready());
  assert(particles);

  if (particles->count == 0)
    return;

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  push_command(state, state.blendcommand, bind_pipeline_command(context.particleblendpipeline));

  push_command(state, state.blendcommand, bind_vertexbuffer_command(0, context.unitquad));

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(ParticleMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(ParticleMaterialSet));

    bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, particlesystem->spritesheet->texture);

    push_command(state, state.blendcommand, bind_descriptor_command(ShaderLocation::materialset, state.materialset, offset));
  }

  if (state.modelset.available() < particles->count*sizeof(Particle))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, particles->count*sizeof(Particle), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(particles->count*sizeof(Particle));

    auto modelset = state.modelset.memory<Particle>(offset);

    for(int i = 0; i < particles->count; ++i)
    {
      modelset[i].position = Vec4(transform * particles->position[i], particles->layer[i] - 0.5f + 1e-3f);
      modelset[i].transform = particles->transform[i];
      modelset[i].color = particles->color[i];
    }

    push_command(state, state.blendcommand, bind_descriptor_command(ShaderLocation::modelset, state.modelset, offset));

    push_command(state, state.blendcommand, draw_command(context.unitquad.vertexcount, particles->count));
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
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());
  assert(envmap && envmap->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  push_command(state, state.colorcommand, bind_pipeline_command(context.waterpipeline));

  push_command(state, state.colorcommand, bind_vertexbuffer_command(0, mesh->vertexbuffer));

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(WaterMaterialSet), std::move(state.materialset));

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
    bind_texture(context.vulkan, state.materialset, ShaderLocation::surfacemap, envmap->texture);
    bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

    push_command(state, state.colorcommand, bind_descriptor_command(ShaderLocation::materialset, state.materialset, offset));
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

    push_command(state, state.colorcommand, bind_descriptor_command(ShaderLocation::modelset, state.modelset, offset));

    push_command(state, state.colorcommand, draw_indexed_command(mesh->vertexbuffer.indexcount, 1));
  }
}


///////////////////////// ForwardList::push_fogplane ////////////////////////
void ForwardList::push_fogplane(ForwardList::BuildState &state, Color4 const &color, Plane const &plane, float density, float startdistance, float falloff)
{
  assert(state.commandlump);

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  push_command(state, state.colorcommand, bind_pipeline_command(context.fogpipeline));

  push_command(state, state.colorcommand, bind_vertexbuffer_command(0, context.unitquad));

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(FogPlaneMaterialSet), std::move(state.materialset));

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(FogPlaneMaterialSet));

    auto materialset = state.materialset.memory<FogPlaneMaterialSet>(offset);

    materialset->plane = Vec4(plane.normal, plane.distance);
    materialset->color = color;
    materialset->density = density;
    materialset->falloff = falloff;
    materialset->startdistance = max(startdistance, 0.0f) * 1.4142135f;

    push_command(state, state.colorcommand, bind_descriptor_command(ShaderLocation::materialset, state.materialset, offset));
  }

  if (state.modelset.available() < sizeof(ModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(ModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = Transform::translation(0, 0, max(startdistance, 0.01f));

    push_command(state, state.colorcommand, bind_descriptor_command(ShaderLocation::modelset, state.modelset, offset));

    push_command(state, state.colorcommand, draw_command(context.unitquad.vertexcount, 1));
  }
}


///////////////////////// ForwardList::finalise /////////////////////////////
void ForwardList::finalise(BuildState &state)
{
  assert(state.commandlump);

  state.commandlump = nullptr;
}
