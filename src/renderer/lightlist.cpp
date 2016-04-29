//
// Datum - light list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "lightlist.h"
#include "renderer.h"
#include "debug.h"

using namespace std;
using namespace lml;
using namespace Vulkan;
using leap::alignto;
using leap::extentof;

enum ShaderLocation
{
  sceneset = 0,
  environmentset = 1,
  materialset = 2,
  modelset = 3,
  computeset = 4,

  albedomap = 1,
  specularmap = 2,
  normalmap = 3,
  depthmap = 4,
};

struct alignas(16) MainLight
{
  Vec4 direction;
  Color4 intensity;
};

struct alignas(16) PointLight
{
  Vec4 position;
  Color4 intensity;
  Vec4 attenuation;
};

struct EnvironmentSet
{
  MainLight mainlight;

  uint32_t pointlightcount;
  PointLight pointlights[256];
};


///////////////////////// draw_lights ////////////////////////////////////////
void draw_lights(RenderContext &context, VkCommandBuffer commandbuffer, PushBuffer const &renderables, RenderParams const &params)
{
  assert(sizeof(EnvironmentSet) < context.lightingbuffersize);

  auto offset = context.lightingbufferoffsets[context.frame & 1];

  EnvironmentSet *environment = (EnvironmentSet*)(context.transfermemory + offset);

  auto &mainlight = environment->mainlight;
  auto &pointlightcount = environment->pointlightcount;
  auto &pointlights = environment->pointlights;

  mainlight.direction.xyz = params.sundirection;
  mainlight.intensity.rgb = params.sunintensity;

  pointlightcount = 0;

  for(auto &renderable : renderables)
  {
    if (renderable.type == Renderable::Type::Lights)
    {
      auto lights = (EnvironmentSet*)renderable_cast<Renderable::Lights>(&renderable)->commandlist->lookup(ShaderLocation::environmentset);

      for(size_t i = 0; lights && i < lights->pointlightcount && pointlightcount < extentof(pointlights); ++i)
      {
        pointlights[pointlightcount].position = lights->pointlights[i].position;
        pointlights[pointlightcount].intensity = lights->pointlights[i].intensity;
        pointlights[pointlightcount].attenuation = lights->pointlights[i].attenuation;
        pointlights[pointlightcount].attenuation.w = params.lightfalloff * lights->pointlights[i].attenuation.w;

        ++pointlightcount;
      }
    }
  }

  bindresource(commandbuffer, context.sceneset, context.pipelinelayout, ShaderLocation::sceneset, VK_PIPELINE_BIND_POINT_COMPUTE);

  bindresource(commandbuffer, context.lightingbuffer, context.pipelinelayout, ShaderLocation::environmentset, offset, VK_PIPELINE_BIND_POINT_COMPUTE);
}



//|---------------------- LightList -----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// LightList::begin //////////////////////////////////
bool LightList::begin(BuildState &state, DatumPlatform::v1::PlatformInterface &platform, RenderContext &context, ResourceManager *resources)
{
  m_commandlist = nullptr;

  state = {};
  state.context = &context;
  state.resources = resources;

  if (!context.framebuffer)
    return false;

  auto commandlist = resources->allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  auto environmentset = commandlist->acquire(ShaderLocation::environmentset, context.environmentsetlayout, sizeof(EnvironmentSet));

  if (environmentset)
  {
    auto environment = environmentset.memory<EnvironmentSet>();

    environment->pointlightcount = 0;

    state.data = environment;

    commandlist->release(environmentset, sizeof(EnvironmentSet));
  }

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// LightList::push_pointlight ////////////////////////
void LightList::push_pointlight(BuildState &state, Vec3 const &position, float range, Color3 const &intensity, Attenuation const &attenuation)
{
  EnvironmentSet *environment = (EnvironmentSet*)state.data;

  if (environment && environment->pointlightcount < extentof(environment->pointlights))
  {
    PointLight &pointlight = environment->pointlights[environment->pointlightcount];

    pointlight.position.xyz = position;
    pointlight.intensity.rgb = intensity;
    pointlight.attenuation.x = attenuation.quadratic;
    pointlight.attenuation.y = attenuation.linear;
    pointlight.attenuation.z = attenuation.constant;
    pointlight.attenuation.w = range;

    environment->pointlightcount += 1;
  }
}


///////////////////////// LightList::finalise ///////////////////////////////
void LightList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist = nullptr;
}
