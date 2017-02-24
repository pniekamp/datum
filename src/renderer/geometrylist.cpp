//
// Datum - geometry list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "geometrylist.h"
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

struct OceanMaterialSet
{
  Color4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;
  Vec2 flow;
};

struct GeometryMaterialSet
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


///////////////////////// draw_geometry /////////////////////////////////////
void draw_geometry(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Geometry const &geometry)
{
  execute(commandbuffer, geometry.commandlist->commandbuffer());
}


//|---------------------- GeometryList --------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// GeometryList::begin ///////////////////////////////
bool GeometryList::begin(BuildState &state, PlatformInterface &platform, RenderContext &context, ResourceManager *resources)
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

  bindresource(*commandlist, context.scenedescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// GeometryList::push_mesh ///////////////////////////
void GeometryList::push_mesh(BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material)
{
  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  if (state.pipeline != context.geometrypipeline)
  {
    bindresource(commandlist, context.geometrypipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.geometrypipeline;
  }

  if (!mesh)
    return;

  if (state.mesh != mesh)
  {
    if (!mesh->ready())
    {
      state.resources->request(*state.platform, mesh);

      if (!mesh->ready())
        return;
    }

    bindresource(commandlist, mesh->vertexbuffer);

    state.mesh = mesh;
  }

  if (!material)
    return;

  if (state.material != material)
  {
    if (!material->ready())
    {
      state.resources->request(*state.platform, material);

      if (!material->ready())
        return;
    }

    state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(GeometryMaterialSet), state.materialset);

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(GeometryMaterialSet));

      auto materialset = state.materialset.memory<GeometryMaterialSet>(offset);

      materialset->color = material->color;
      materialset->metalness = material->metalness;
      materialset->roughness = material->roughness;
      materialset->reflectivity = material->reflectivity;
      materialset->emissive = material->emissive;

      bindtexture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
      bindtexture(context.vulkan, state.materialset, ShaderLocation::specularmap, material->specularmap ? material->specularmap->texture : context.whitediffuse);
      bindtexture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

      bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
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


///////////////////////// GeometryList::push_ocean //////////////////////////
void GeometryList::push_ocean(GeometryList::BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material, Vec2 const &flow)
{
  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  state.pipeline = nullptr;

  bindresource(commandlist, context.oceanpipeline[0], 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  if (!mesh)
    return;

  if (state.mesh != mesh)
  {
    if (!mesh->ready())
    {
      state.resources->request(*state.platform, mesh);

      if (!mesh->ready())
        return;
    }

    bindresource(commandlist, mesh->vertexbuffer);

    state.mesh = mesh;
  }

  if (!material)
    return;

  if (state.material != material)
  {
    if (!material->ready())
    {
      state.resources->request(*state.platform, material);

      if (!material->ready())
        return;
    }

    state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(OceanMaterialSet), state.materialset);

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(OceanMaterialSet));

      auto materialset = state.materialset.memory<OceanMaterialSet>(offset);

      materialset->color = material->color;
      materialset->metalness = material->metalness;
      materialset->roughness = material->roughness;
      materialset->reflectivity = material->reflectivity;
      materialset->emissive = material->emissive;
      materialset->flow = flow;

      bindtexture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);
      bindtexture(context.vulkan, state.materialset, ShaderLocation::specularmap, material->specularmap ? material->specularmap->texture : context.whitediffuse);
      bindtexture(context.vulkan, state.materialset, ShaderLocation::normalmap, material->normalmap ? material->normalmap->texture : context.nominalnormal);

      bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  setimagelayout(commandlist, context.depthbuffer.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });

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

  setimagelayout(commandlist, context.depthbuffer.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });

  bindresource(commandlist, context.oceanpipeline[1], 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  draw(commandlist, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
}


///////////////////////// GeometryList::finalise ////////////////////////////
void GeometryList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist->release(state.modelset);
  state.commandlist->release(state.materialset);

  state.commandlist->end();

  state.commandlist = nullptr;
}
