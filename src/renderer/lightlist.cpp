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
using leap::extentof;

//|---------------------- LightList -----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// LightList::begin //////////////////////////////////
bool LightList::begin(BuildState &state, RenderContext &context, ResourceManager &resources)
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

  auto lightset = commandlump->acquire_descriptor(sizeof(Renderable::Lights::LightList));

  if (lightset.capacity() == 0)
  {
    resources.destroy(commandlump);
    return false;
  }

  lightlist = lightset.memory<Renderable::Lights::LightList>(lightset.reserve(sizeof(Renderable::Lights::LightList)));

  lightlist->pointlightcount = 0;
  lightlist->spotlightcount = 0;
  lightlist->probecount = 0;
  lightlist->environmentcount = 0;

  m_commandlump = { resources, commandlump };

  state.commandlump = commandlump;

  return true;
}


///////////////////////// LightList::push_pointlight ////////////////////////
void LightList::push_pointlight(BuildState &state, Vec3 const &position, float range, Color3 const &intensity, Attenuation const &attenuation)
{
  if (lightlist && lightlist->pointlightcount < extentof(lightlist->pointlights))
  {
    auto &entry = lightlist->pointlights[lightlist->pointlightcount];

    entry.position = position;
    entry.intensity = intensity;
    entry.attenuation.x = attenuation.quadratic;
    entry.attenuation.y = attenuation.linear;
    entry.attenuation.z = attenuation.constant;
    entry.attenuation.w = range;

    lightlist->pointlightcount += 1;
  }
}


///////////////////////// LightList::push_spotlight /////////////////////////
void LightList::push_spotlight(BuildState &state, Vec3 const &position, Vec3 const &direction, float cutoff, float range, Color3 const &intensity, Attenuation const &attenuation, lml::Transform const &spotview, SpotMap const *spotmap)
{
  if (lightlist && lightlist->spotlightcount < extentof(lightlist->spotlights))
  {
    auto &entry = lightlist->spotlights[lightlist->spotlightcount];

    entry.position = position;
    entry.intensity = intensity;
    entry.attenuation.x = attenuation.quadratic;
    entry.attenuation.y = attenuation.linear;
    entry.attenuation.z = attenuation.constant;
    entry.attenuation.w = range;
    entry.direction = direction;
    entry.cutoff = 1 - cutoff;
    entry.spotview = spotview;
    entry.spotmap = spotmap;

    lightlist->spotlightcount += 1;
  }
}


///////////////////////// LightList::push_probe /////////////////////////////
void LightList::push_probe(BuildState &state, Vec3 const &position, float radius, Irradiance const &irradiance)
{
  if (lightlist && lightlist->probecount < extentof(lightlist->probes))
  {
    auto &entry = lightlist->probes[lightlist->probecount];

    entry.position = Vec4(position, radius);
    entry.irradiance = irradiance;

    lightlist->probecount += 1;
  }
}


///////////////////////// LightList::push_environment ///////////////////////
void LightList::push_environment(BuildState &state, Transform const &transform, Vec3 const &size, EnvMap const *envmap)
{
  assert(envmap && envmap->ready());

  if (lightlist && lightlist->environmentcount < extentof(lightlist->environments))
  {
    auto &entry = lightlist->environments[lightlist->environmentcount];

    entry.size = size;
    entry.transform = transform;
    entry.envmap = envmap;

    lightlist->environmentcount += 1;
  }
}


///////////////////////// LightList::finalise ///////////////////////////////
void LightList::finalise(BuildState &state)
{
  assert(state.commandlump);

  state.commandlump = nullptr;
}
