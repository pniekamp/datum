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

  if (!context.prepared)
    return false;

  auto commandlist = resources.allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  if (auto sceneset = commandlist->acquire(context.modelsetlayout, sizeof(Renderable::Lights::LightList)))
  {
    auto offset = sceneset.reserve(sizeof(Renderable::Lights::LightList));

    m_lights = sceneset.memory<Renderable::Lights::LightList>(offset);

    m_lights->pointlightcount = 0;
    m_lights->spotlightcount = 0;

    commandlist->release(sceneset);
  }

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// LightList::push_pointlight ////////////////////////
void LightList::push_pointlight(BuildState &state, Vec3 const &position, float range, Color3 const &intensity, Attenuation const &attenuation)
{
  if (m_lights && m_lights->pointlightcount < extentof(m_lights->pointlights))
  {
    auto &pointlight = m_lights->pointlights[m_lights->pointlightcount];

    pointlight.position = position;
    pointlight.intensity = intensity;
    pointlight.attenuation.x = attenuation.quadratic;
    pointlight.attenuation.y = attenuation.linear;
    pointlight.attenuation.z = attenuation.constant;
    pointlight.attenuation.w = range;

    m_lights->pointlightcount += 1;
  }
}


///////////////////////// LightList::push_spotlight /////////////////////////
void LightList::push_spotlight(BuildState &state, Vec3 const &position, Vec3 const &direction, float cutoff, float range, Color3 const &intensity, Attenuation const &attenuation)
{
  if (m_lights && m_lights->spotlightcount < extentof(m_lights->spotlights))
  {
    auto &spotlight = m_lights->spotlights[m_lights->spotlightcount];

    spotlight.position = position;
    spotlight.intensity = intensity;
    spotlight.attenuation.x = attenuation.quadratic;
    spotlight.attenuation.y = attenuation.linear;
    spotlight.attenuation.z = attenuation.constant;
    spotlight.attenuation.w = range;
    spotlight.direction = direction;
    spotlight.cutoff = 1 - cutoff;

    m_lights->spotlightcount += 1;
  }
}


///////////////////////// LightList::finalise ///////////////////////////////
void LightList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist = nullptr;
}
