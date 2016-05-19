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
using leap::alignto;
using leap::extentof;

enum ShaderLocation
{
  sceneset = 0,
};


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

  auto sceneset = commandlist->acquire(ShaderLocation::sceneset, context.scenesetlayout, sizeof(Lights));

  if (sceneset)
  {
    auto offset = sceneset.reserve(sizeof(Lights));

    state.lights = sceneset.memory<Lights>(offset);

    state.lights->pointlightcount = 0;

    commandlist->release(sceneset);
  }

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// LightList::push_pointlight ////////////////////////
void LightList::push_pointlight(BuildState &state, Vec3 const &position, float range, Color3 const &intensity, Attenuation const &attenuation)
{
  if (state.lights && state.lights->pointlightcount < extentof(state.lights->pointlights))
  {
    auto &pointlight = state.lights->pointlights[state.lights->pointlightcount];

    pointlight.position.xyz = position;
    pointlight.intensity.rgb = intensity;
    pointlight.attenuation.x = attenuation.quadratic;
    pointlight.attenuation.y = attenuation.linear;
    pointlight.attenuation.z = attenuation.constant;
    pointlight.attenuation.w = range;

    state.lights->pointlightcount += 1;
  }
}


///////////////////////// LightList::finalise ///////////////////////////////
void LightList::finalise(BuildState &state)
{
  assert(state.commandlist);

  state.commandlist = nullptr;
}
