//
// Datum - geometry list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "geometrylist.h"
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

  albedomap = 1,
  surfacemap = 2,
  normalmap = 3,
};

struct OceanMaterialSet
{
  alignas(16) Color4 color;
  alignas( 4) float metalness;
  alignas( 4) float roughness;
  alignas( 4) float reflectivity;
  alignas( 4) float emissive;
  alignas(16) Vec3 bumpscale;
  alignas(16) Vec4 foamplane;
  alignas( 4) float foamwaveheight;
  alignas( 4) float foamwavescale;
  alignas( 4) float foamshoreheight;
  alignas( 4) float foamshorescale;
  alignas(16) Vec2 flow;
};

struct GeometryMaterialSet
{
  alignas(16) Color4 color;
  alignas( 4) float metalness;
  alignas( 4) float roughness;
  alignas( 4) float reflectivity;
  alignas( 4) float emissive;
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


///////////////////////// draw_prepass //////////////////////////////////////
void draw_prepass(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Geometry const &geometry)
{
  execute(commandbuffer, geometry.prepasscommands);
}

///////////////////////// draw_geometry /////////////////////////////////////
void draw_geometry(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Geometry const &geometry)
{
  execute(commandbuffer, geometry.geometrycommands);
}


//|---------------------- GeometryList --------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// GeometryList::begin ///////////////////////////////
bool GeometryList::begin(BuildState &state, RenderContext &context, ResourceManager &resources)
{
  m_commandlist = {};

  state = {};
  state.context = &context;
  state.resources = &resources;

  if (!context.ready)
    return false;

  auto commandlist = resources.allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  prepasscommands = commandlist->allocate_commandbuffer();
  geometrycommands = commandlist->allocate_commandbuffer();

  if (!prepasscommands || !geometrycommands)
  {
    resources.destroy(commandlist);
    return false;
  }

  using Vulkan::begin;

  begin(context.vulkan, prepasscommands, context.preframebuffer, context.prepass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
  begin(context.vulkan, geometrycommands, context.geometryframebuffer, context.geometrypass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::sceneset, context.scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::sceneset, context.scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// GeometryList::push_mesh ///////////////////////////
void GeometryList::push_mesh(BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  if (state.pipeline != context.modelgeometrypipeline)
  {
    bind_pipeline(prepasscommands, context.modelprepasspipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_pipeline(geometrycommands, context.modelgeometrypipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.modelgeometrypipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(prepasscommands, 0, mesh->vertexbuffer);
    bind_vertexbuffer(geometrycommands, 0, mesh->vertexbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    state.materialset = commandlist.acquire_descriptor(context.materialsetlayout, sizeof(GeometryMaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(GeometryMaterialSet));

      auto materialset = state.materialset.memory<GeometryMaterialSet>(offset);

      materialset->color = material->color;
      materialset->metalness = material->metalness;
      materialset->roughness = material->roughness;
      materialset->reflectivity = material->reflectivity;
      materialset->emissive = material->emissive;

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
      bind_texture(context.vulkan, state.materialset, ShaderLocation::surfacemap, material->surfacemap ? material->surfacemap->texture : context.whitediffuse);
      bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

      bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
      bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  if (state.modelset.available() < sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire_descriptor(context.modelsetlayout, sizeof(ModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(prepasscommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
    draw(geometrycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// GeometryList::push_mesh ///////////////////////////
void GeometryList::push_mesh(BuildState &state, Transform const &transform, Pose const &pose, Mesh const *mesh, Material const *material)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  if (state.pipeline != context.actorgeometrypipeline)
  {
    bind_pipeline(prepasscommands, context.actorprepasspipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_pipeline(geometrycommands, context.actorgeometrypipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.actorgeometrypipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(prepasscommands, 0, mesh->vertexbuffer);
    bind_vertexbuffer(geometrycommands, 0, mesh->vertexbuffer);
    bind_vertexbuffer(prepasscommands, 1, mesh->rigbuffer);
    bind_vertexbuffer(geometrycommands, 1, mesh->rigbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    state.materialset = commandlist.acquire_descriptor(context.materialsetlayout, sizeof(GeometryMaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(GeometryMaterialSet));

      auto materialset = state.materialset.memory<GeometryMaterialSet>(offset);

      materialset->color = material->color;
      materialset->metalness = material->metalness;
      materialset->roughness = material->roughness;
      materialset->reflectivity = material->reflectivity;
      materialset->emissive = material->emissive;

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
      bind_texture(context.vulkan, state.materialset, ShaderLocation::surfacemap, material->surfacemap ? material->surfacemap->texture : context.whitediffuse);
      bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

      bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
      bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  if (state.modelset.available() < sizeof(ActorSet) + pose.bonecount*sizeof(Transform))
  {
    state.modelset = commandlist.acquire_descriptor(context.modelsetlayout, sizeof(ActorSet) + pose.bonecount*sizeof(Transform), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ActorSet) + pose.bonecount*sizeof(Transform));

    auto modelset = state.modelset.memory<ActorSet>(offset);

    modelset->modelworld = transform;

    copy(pose.bones, pose.bones + pose.bonecount, modelset->bones);

    bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(prepasscommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
    draw(geometrycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// GeometryList::push_ocean //////////////////////////
void GeometryList::push_ocean(GeometryList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, Vec2 const &flow, Vec3 const &bumpscale, const Plane &foamplane, float foamwaveheight, float foamwavescale, float foamshoreheight, float foamshorescale)
{
  assert(state.commandlist);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  if (state.pipeline != context.oceanpipeline)
  {
    bind_pipeline(geometrycommands, context.oceanpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.oceanpipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(geometrycommands, 0, mesh->vertexbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    state.materialset = commandlist.acquire_descriptor(context.materialsetlayout, sizeof(OceanMaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(OceanMaterialSet));

      auto materialset = state.materialset.memory<OceanMaterialSet>(offset);

      materialset->color = material->color;
      materialset->metalness = material->metalness;
      materialset->roughness = material->roughness;
      materialset->reflectivity = material->reflectivity;
      materialset->emissive = material->emissive;
      materialset->bumpscale = Vec3(bumpscale.xy, 1.0f / (0.01f + bumpscale.z));
      materialset->foamplane = Vec4(foamplane.normal, foamplane.distance);
      materialset->foamwaveheight = foamwaveheight;
      materialset->foamwavescale = foamwavescale;
      materialset->foamshoreheight = foamshoreheight;
      materialset->foamshorescale = foamshorescale;
      materialset->flow = flow;

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
      bind_texture(context.vulkan, state.materialset, ShaderLocation::surfacemap, material->surfacemap ? material->surfacemap->texture : context.whitediffuse);
      bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

      bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  if (state.modelset.available() < sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire_descriptor(context.modelsetlayout, sizeof(ModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(geometrycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// GeometryList::finalise ////////////////////////////
void GeometryList::finalise(BuildState &state)
{
  assert(state.commandlist);

  auto &context = *state.context;

  end(context.vulkan, prepasscommands);
  end(context.vulkan, geometrycommands);

  state.commandlist = nullptr;
}
