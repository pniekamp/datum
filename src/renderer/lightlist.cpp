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
using leap::alignto;
using leap::extentof;

enum ShaderLocation
{
  sceneset = 0,
};

//|---------------------- LightList -----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// LightList::begin //////////////////////////////////
bool LightList::begin(BuildState &state, RenderContext &context, ResourceManager *resources)
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

  if (auto sceneset = commandlist->acquire(context.scenesetlayout, sizeof(Renderable::Lights::LightList)))
  {
    auto offset = sceneset.reserve(sizeof(Renderable::Lights::LightList));

    m_lights = sceneset.memory<Renderable::Lights::LightList>(offset);

    m_lights->pointlightcount = 0;

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


///////////////////////// LightList::finalise ///////////////////////////////
void LightList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist = nullptr;
}
