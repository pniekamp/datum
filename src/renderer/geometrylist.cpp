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
  extendedset = 3,

  albedomap = 1,
  surfacemap = 2,
  normalmap = 3,

  blendmap = 0,
};

struct MaterialSet
{
  alignas(16) Color4 color;
  alignas( 4) float metalness;
  alignas( 4) float roughness;
  alignas( 4) float reflectivity;
  alignas( 4) float emissive;
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

struct TerrainSet
{
  alignas(16) Transform modelworld;
  alignas(16) Vec2 uvscale;
  alignas( 4) uint32_t layers;
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
  m_commandlump = {};

  state = {};
  state.context = &context;
  state.resources = &resources;

  if (!context.ready)
    return false;

  auto commandlump = resources.allocate<CommandLump>(&context);

  if (!commandlump)
    return false;

  prepasscommands = commandlump->allocate_commandbuffer();
  geometrycommands = commandlump->allocate_commandbuffer();

  if (!prepasscommands || !geometrycommands)
  {
    resources.destroy(commandlump);
    return false;
  }

  using Vulkan::begin;

  begin(context.vulkan, prepasscommands, context.preframebuffer, context.prepass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
  begin(context.vulkan, geometrycommands, context.geometryframebuffer, context.geometrypass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::sceneset, context.scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::sceneset, context.scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlump = { resources, commandlump };

  state.commandlump = commandlump;

  return true;
}


///////////////////////// GeometryList::bind_material ///////////////////////
void GeometryList::bind_material(BuildState &state, Material const *material)
{
  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.materialset.available() < sizeof(MaterialSet) || !state.material || state.material->albedomap != material->albedomap || state.material->surfacemap != material->surfacemap || state.material->normalmap != material->normalmap)
  {
    state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
      bind_texture(context.vulkan, state.materialset, ShaderLocation::surfacemap, material->surfacemap ? material->surfacemap->texture : context.whitediffuse);
      bind_texture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);
    }
  }

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaterialSet));

    auto materialset = state.materialset.memory<MaterialSet>(offset);

    materialset->color = material->color;
    materialset->metalness = material->metalness;
    materialset->roughness = material->roughness;
    materialset->reflectivity = material->reflectivity;
    materialset->emissive = material->emissive;

    bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
  }
}


///////////////////////// GeometryList::push_mesh ///////////////////////////
void GeometryList::push_mesh(BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

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
    bind_material(state, material);

    state.material = material;
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

    bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(prepasscommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
    draw(geometrycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// GeometryList::push_mesh ///////////////////////////
void GeometryList::push_mesh(BuildState &state, Transform const &transform, Pose const &pose, Mesh const *mesh, Material const *material)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

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
    bind_material(state, material);

    state.material = material;
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

    bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(prepasscommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
    draw(geometrycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// GeometryList::push_foilage ////////////////////////
void GeometryList::push_foilage(BuildState &state, Transform const *transforms, size_t count, Mesh const *mesh, Material const *material, Vec4 const &wind, Vec3 const &bendscale, Vec3 const &detailbendscale)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.pipeline != context.foilagegeometrypipeline)
  {
    bind_pipeline(prepasscommands, context.foilageprepasspipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_pipeline(geometrycommands, context.foilagegeometrypipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.foilagegeometrypipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(prepasscommands, 0, mesh->vertexbuffer);
    bind_vertexbuffer(geometrycommands, 0, mesh->vertexbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    bind_material(state, material);

    state.material = material;
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

    bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(prepasscommands, mesh->vertexbuffer.indexcount, count, 0, 0, 0);
    draw(geometrycommands, mesh->vertexbuffer.indexcount, count, 0, 0, 0);
  }
}


///////////////////////// GeometryList::push_terrain ////////////////////////
void GeometryList::push_terrain(BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, ::Texture const *blendmap, int layers, Vec2 const &uvscale)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready() && material->albedomap && material->surfacemap && material->normalmap);
  assert(blendmap && blendmap->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.pipeline != context.terrainpipeline)
  {
    bind_pipeline(prepasscommands, context.modelprepasspipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_pipeline(geometrycommands, context.terrainpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.terrainpipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(prepasscommands, 0, mesh->vertexbuffer);
    bind_vertexbuffer(geometrycommands, 0, mesh->vertexbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    bind_material(state, material);

    state.material = material;
  }

  if (state.modelset.available() < sizeof(TerrainSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(TerrainSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(TerrainSet));

    auto modelset = state.modelset.memory<TerrainSet>(offset);

    modelset->modelworld = transform;
    modelset->uvscale = uvscale;
    modelset->layers = layers;

    bind_descriptor(prepasscommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    auto extendedset = commandlump.acquire_descriptor(context.extendedsetlayout);

    bind_texture(context.vulkan, extendedset, ShaderLocation::blendmap, blendmap->texture);

    bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::extendedset, extendedset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(prepasscommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
    draw(geometrycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// GeometryList::push_ocean //////////////////////////
void GeometryList::push_ocean(GeometryList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, Vec2 const &flow, Vec3 const &bumpscale, Plane const &foamplane, float foamwaveheight, float foamwavescale, float foamshoreheight, float foamshorescale)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

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

  state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(OceanMaterialSet), std::move(state.materialset));

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

    state.material = nullptr;
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

    bind_descriptor(geometrycommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(geometrycommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// GeometryList::finalise ////////////////////////////
void GeometryList::finalise(BuildState &state)
{
  assert(state.commandlump);

  auto &context = *state.context;

  end(context.vulkan, prepasscommands);
  end(context.vulkan, geometrycommands);

  state.commandlump = nullptr;
}
