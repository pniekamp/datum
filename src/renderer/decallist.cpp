//
// Datum - decal list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "decallist.h"
#include "debug.h"

using namespace std;
using namespace lml;
using namespace Vulkan;
using leap::alignto;
using leap::extentof;

//|---------------------- DecalList -----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// DecalList::begin //////////////////////////////////
bool DecalList::begin(BuildState &state, RenderContext &context, ResourceManager &resources)
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

  auto decalset = commandlump->acquire_descriptor(sizeof(Renderable::Decals::DecalList));

  if (decalset.capacity() == 0)
  {
    resources.destroy(commandlump);
    return false;
  }

  decallist = decalset.memory<Renderable::Decals::DecalList>(decalset.reserve(sizeof(Renderable::Decals::DecalList)));

  decallist->decalcount = 0;

  m_commandlump = { resources, commandlump };

  state.commandlump = commandlump;

  return true;
}


///////////////////////// DecalList::push_decal /////////////////////////
void DecalList::push_decal(BuildState &state, Transform const &transform, Vec3 const &size, Decal const *decal, Color4 const &tint)
{
  push_decal(state, transform, size, decal, 0, tint);
}


///////////////////////// DecalList::push_decal /////////////////////////
void DecalList::push_decal(BuildState &state, Transform const &transform, Vec3 const &size, Decal const *decal, float layer, Color4 const &tint)
{
  assert(decal && decal->ready());

  if (decallist && decallist->decalcount < extentof(decallist->decals))
  {
    auto &entry = decallist->decals[decallist->decalcount];

    entry.size = size;
    entry.transform = transform;
    entry.material = decal->material;
    entry.extent = decal->extent;
    entry.layer = layer;
    entry.tint = tint;
    entry.mask = state.decalmask;

    decallist->decalcount += 1;
  }
}


///////////////////////// DecalList::finalise ///////////////////////////////
void DecalList::finalise(BuildState &state)
{
  assert(state.commandlump);

  state.commandlump = nullptr;
}
