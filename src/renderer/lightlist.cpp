//
// Datum - light list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "lightlist.h"
#include "debug.h"

using namespace std;
using namespace lml;
using namespace Vulkan;
using leap::alignto;
using leap::extentof;


//|---------------------- LightList -----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// LightList::begin //////////////////////////////////
bool LightList::begin(BuildState &state, RenderContext &context, ResourceManager &resources)
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

  auto lightset = commandlist->acquire_descriptor(sizeof(Renderable::Lights::LightList));

  if (lightset.capacity() == 0)
  {
    resources.destroy(commandlist);
    return false;
  }

  lightlist = lightset.memory<Renderable::Lights::LightList>(lightset.reserve(sizeof(Renderable::Lights::LightList)));

  lightlist->pointlightcount = 0;
  lightlist->spotlightcount = 0;
  lightlist->environmentcount = 0;

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// LightList::push_pointlight ////////////////////////
void LightList::push_pointlight(BuildState &state, Vec3 const &position, float range, Color3 const &intensity, Attenuation const &attenuation)
{
  if (lightlist && lightlist->pointlightcount < extentof(lightlist->pointlights))
  {
    auto &pointlight = lightlist->pointlights[lightlist->pointlightcount];

    pointlight.position = position;
    pointlight.intensity = intensity;
    pointlight.attenuation.x = attenuation.quadratic;
    pointlight.attenuation.y = attenuation.linear;
    pointlight.attenuation.z = attenuation.constant;
    pointlight.attenuation.w = range;

    lightlist->pointlightcount += 1;
  }
}


///////////////////////// LightList::push_spotlight /////////////////////////
void LightList::push_spotlight(BuildState &state, Vec3 const &position, Vec3 const &direction, float cutoff, float range, Color3 const &intensity, Attenuation const &attenuation)
{
  if (lightlist && lightlist->spotlightcount < extentof(lightlist->spotlights))
  {
    auto &spotlight = lightlist->spotlights[lightlist->spotlightcount];

    spotlight.position = position;
    spotlight.intensity = intensity;
    spotlight.attenuation.x = attenuation.quadratic;
    spotlight.attenuation.y = attenuation.linear;
    spotlight.attenuation.z = attenuation.constant;
    spotlight.attenuation.w = range;
    spotlight.direction = direction;
    spotlight.cutoff = 1 - cutoff;

    lightlist->spotlightcount += 1;
  }
}


///////////////////////// LightList::push_environment ///////////////////////
void LightList::push_environment(BuildState &state, Transform const &transform, Vec3 const &dimension, EnvMap const *envmap)
{
  assert(envmap && envmap->ready());

  if (lightlist && lightlist->environmentcount < extentof(lightlist->environments))
  {
    auto &environment = lightlist->environments[lightlist->environmentcount];

    environment.dimension = dimension;
    environment.transform = transform;
    environment.envmap = envmap;

    lightlist->environmentcount += 1;
  }
}


///////////////////////// LightList::finalise ///////////////////////////////
void LightList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist = nullptr;
}
