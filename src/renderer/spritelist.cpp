//
// Datum - sprite list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "spritelist.h"
#include "renderer.h"
#include "leap/lml/matrix.h"
#include "debug.h"

using namespace std;
using namespace lml;
using namespace Vulkan;
using namespace DatumPlatform;
using leap::alignto;
using leap::extentof;

enum RenderPasses
{
  spritepass = 0,
};

enum ShaderLocation
{
  sceneset = 0,
  modelset = 1,
};

struct MaterialSet
{
  Vec4 texcoords;
  Color4 tint;
};

struct ModelSet
{
  Vec2 xbasis;
  Vec2 ybasis;
  Vec4 position;
};


//|---------------------- SpriteList ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// SpriteList::begin /////////////////////////////////
bool SpriteList::begin(BuildState &state, PlatformInterface &platform, RenderContext &context, ResourceManager *resources)
{
  m_commandlist = nullptr;

  state = {};
  state.platform = &platform;
  state.context = &context;
  state.resources = resources;

  if (!context.framebuffer)
    return false;

  auto commandlist = resources->allocate<CommandList>();

  if (!commandlist)
    return false;

  if (!commandlist->begin(context, context.framebuffer, context.renderpass, RenderPasses::spritepass))
  {
    resources->destroy(commandlist);
    return false;
  }

  bindresource(*commandlist, context.spritepipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(*commandlist, context.sceneset, context.pipelinelayout, ShaderLocation::sceneset, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(*commandlist, context.unitquad);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  return true;
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(SpriteList::BuildState &state, lml::Vec2 xbasis, lml::Vec2 ybasis, lml::Vec2 position)
{
  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  if (state.modelset.capacity() < state.offset + sizeof(ModelSet))
  {
    if (state.modelset)
      commandlist.release(state.modelset, state.offset);

    state.offset = 0;
    state.modelset = commandlist.acquire(context.modelsetlayout, sizeof(ModelSet));
  }

  if (state.offset + sizeof(ModelSet) <= state.modelset.capacity())
  {
    auto modelset = state.modelset.memory<ModelSet>(state.offset);

    modelset->xbasis = xbasis;
    modelset->ybasis = ybasis;
    modelset->position = Vec4(position, 0, 1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, state.offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, context.unitquad.vertexcount, 1, 0, 0);

    state.offset = alignto(state.offset + sizeof(ModelSet), state.modelset.alignment());
  }
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(SpriteList::BuildState &state, Vec2 const &position, Rect2 const &rect)
{  
  auto xbasis = Vec2(rect.max.x - rect.min.x, 0);
  auto ybasis = Vec2(0, rect.max.y - rect.min.y);

  push_sprite(state, xbasis, ybasis, position + rect.min);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(SpriteList::BuildState &state, Vec2 const &position, Rect2 const &rect, float rotation)
{
  auto xbasis = rotate(Vec2(rect.max.x - rect.min.x, 0), rotation);
  auto ybasis = rotate(Vec2(0, rect.max.y - rect.min.y), rotation);

  push_sprite(state, xbasis, ybasis, position + rotate(rect.min, rotation));
}


///////////////////////// SpriteList::finalise //////////////////////////////
void SpriteList::finalise(BuildState &state)
{
  assert(state.commandlist);

  auto &commandlist = *state.commandlist;

  if (state.modelset)
    commandlist.release(state.modelset, state.offset);

  state.commandlist->end();

  state.commandlist = nullptr;
}
