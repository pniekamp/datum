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

  albedomap = 1,
  specularmap = 2,
  normalmap = 3,
  depthmap = 4,
  shadowmap = 5,
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

struct SceneSet
{
  Matrix4f proj;
  Matrix4f invproj;
  Matrix4f view;
  Matrix4f invview;
  Vec4 camerapos;

  array<Matrix4f, ShadowMap::nslices> shadowview;

  MainLight mainlight;

  uint32_t pointlightcount;
  PointLight pointlights[256];
};


///////////////////////// draw_lights ////////////////////////////////////////
void draw_lights(RenderContext &context, VkCommandBuffer commandbuffer, PushBuffer const &renderables, RenderParams const &params)
{
  assert(sizeof(SceneSet) < context.lightingbuffersize);

  auto offset = context.lightingbufferoffsets[context.frame & 1];

  SceneSet *scene = (SceneSet*)(context.transfermemory + offset);

  scene->proj = context.proj;
  scene->invproj = inverse(scene->proj);
  scene->view = context.view;
  scene->invview = inverse(scene->view);
  scene->camerapos = Vec4(context.camera.position(), 0);
  scene->shadowview = context.shadows.shadowview;

  auto &mainlight = scene->mainlight;
  auto &pointlightcount = scene->pointlightcount;
  auto &pointlights = scene->pointlights;

  mainlight.direction.xyz = params.sundirection;
  mainlight.intensity.rgb = params.sunintensity;

  pointlightcount = 0;

  for(auto &renderable : renderables)
  {
    if (renderable.type == Renderable::Type::Lights)
    {
      auto lights = renderable_cast<Renderable::Lights>(&renderable)->commandlist->lookup<SceneSet>(ShaderLocation::sceneset);

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

  bindresource(commandbuffer, context.lightingbuffer, context.pipelinelayout, ShaderLocation::sceneset, offset, VK_PIPELINE_BIND_POINT_COMPUTE);
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

  auto sceneset = commandlist->acquire(ShaderLocation::sceneset, context.scenesetlayout, sizeof(SceneSet));

  if (sceneset)
  {
    auto offset = sceneset.reserve(sizeof(SceneSet));

    auto lights = sceneset.memory<SceneSet>(offset);

    lights->pointlightcount = 0;

    commandlist->release(sceneset);

    state.data = lights;
  }

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// LightList::push_pointlight ////////////////////////
void LightList::push_pointlight(BuildState &state, Vec3 const &position, float range, Color3 const &intensity, Attenuation const &attenuation)
{
  SceneSet *scene = (SceneSet*)state.data;

  if (scene && scene->pointlightcount < extentof(scene->pointlights))
  {
    PointLight &pointlight = scene->pointlights[scene->pointlightcount];

    pointlight.position.xyz = position;
    pointlight.intensity.rgb = intensity;
    pointlight.attenuation.x = attenuation.quadratic;
    pointlight.attenuation.y = attenuation.linear;
    pointlight.attenuation.z = attenuation.constant;
    pointlight.attenuation.w = range;

    scene->pointlightcount += 1;
  }
}


///////////////////////// LightList::finalise ///////////////////////////////
void LightList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist = nullptr;
}
