//
// Datum - sprite list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "spritelist.h"
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
  spritepass = 1,
};

enum ShaderLocation
{
  sceneset = 0,
  materialset = 1,
  modelset = 2,

  albedomap = 1,
};

struct SceneSet
{
  Matrix4f worldview;
};

struct MaterialSet
{
  Color4 tint;

  Vec4 texcoords;
};

struct ModelSet
{
  Vec2 xbasis;
  Vec2 ybasis;
  Vec4 position;
};


///////////////////////// draw_sprites //////////////////////////////////////
void draw_sprites(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Sprites const &sprites)
{
  auto scene = sprites.commandlist->lookup<SceneSet>(ShaderLocation::sceneset);

  if (scene)
  {
    scene->worldview = OrthographicProjection(sprites.viewport.min.x, sprites.viewport.min.y, sprites.viewport.max.x, sprites.viewport.max.y, 0.0f, 1000.0f);

    execute(commandbuffer, sprites.commandlist->commandbuffer());
  }
}


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

  auto commandlist = resources->allocate<CommandList>(&context);

  if (!commandlist)
    return false;

  if (!commandlist->begin(context.framebuffer, context.renderpass, RenderPasses::spritepass))
  {
    resources->destroy(commandlist);
    return false;
  }

  bindresource(*commandlist, context.spritepipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  auto sceneset = commandlist->acquire(ShaderLocation::sceneset, context.scenesetlayout, sizeof(SceneSet));

  if (sceneset)
  {
    sceneset.reserve(sizeof(SceneSet));

    bindresource(*commandlist, sceneset, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);

    commandlist->release(sceneset);
  }

  bindresource(*commandlist, context.unitquad);

  m_commandlist = { resources, commandlist };

  state.commandlist = commandlist;

  state.assetbarrier = resources->assets()->acquire_barrier();

  return true;
}


///////////////////////// SpriteList::push_material /////////////////////////
void SpriteList::push_material(BuildState &state, Vulkan::Texture const &texture, Vec4 const &texcoords, Color4 const &tint)
{
  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  if (state.materialset.capacity() < state.materialset.used() + sizeof(MaterialSet) || state.texture != texture)
  {
    state.materialset = commandlist.acquire(ShaderLocation::materialset, context.materialsetlayout, sizeof(MaterialSet), state.materialset);

    if (state.materialset)
    {
      bindtexture(context.device, state.materialset, ShaderLocation::albedomap, texture);

      state.texture = texture;
    }
  }

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaterialSet));

    auto materialset = state.materialset.memory<MaterialSet>(offset);

    materialset->tint = tint;
    materialset->texcoords = texcoords;

    bindresource(commandlist, state.materialset, context.pipelinelayout, ShaderLocation::materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.tint = tint;
    state.texcoords = texcoords;
  }
}


///////////////////////// SpriteList::push_model ////////////////////////////
void SpriteList::push_model(SpriteList::BuildState &state, Vec2 xbasis, Vec2 ybasis, Vec2 position, float layer)
{
  assert(state.commandlist);

  auto &context = *state.context;
  auto &commandlist = *state.commandlist;

  if (state.modelset.capacity() < state.modelset.used() + sizeof(ModelSet))
  {
    state.modelset = commandlist.acquire(ShaderLocation::modelset, context.modelsetlayout, sizeof(ModelSet), state.modelset);
  }

  if (state.modelset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->xbasis = xbasis;
    modelset->ybasis = ybasis;
    modelset->position = Vec4(position, floor(layer), 1);

    bindresource(commandlist, state.modelset, context.pipelinelayout, ShaderLocation::modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(commandlist, context.unitquad.vertexcount, 1, 0, 0);
  }
}


///////////////////////// SpriteList::push_line /////////////////////////////
void SpriteList::push_line(BuildState &state, Vec2 const &a, Vec2 const &b, Color4 const &color, float thickness)
{
  push_rect(state, a, Rect2({ 0, -0.5f*thickness }, { norm(b - a), +0.5f*thickness }), theta(b - a), color);
}


///////////////////////// SpriteList::push_rect /////////////////////////////
void SpriteList::push_rect(BuildState &state, Vec2 const &position, Rect2 const &rect, Color4 const &color)
{
  if (state.texture != state.context->whitediffuse || state.tint != color)
  {
    push_material(state, state.context->whitediffuse, Vec4(0, 0, 1, 1), color);
  }

  auto xbasis = Vec2(1, 0);
  auto ybasis = Vec2(0, 1);

  push_model(state, (rect.max.x - rect.min.x) * xbasis, (rect.max.y - rect.min.y) * ybasis, position + rect.min.x*xbasis + rect.min.y*ybasis, 0);
}


///////////////////////// SpriteList::push_rect /////////////////////////////
void SpriteList::push_rect(BuildState &state, Vec2 const &position, Rect2 const &rect, float rotation, Color4 const &color)
{
  if (state.texture != state.context->whitediffuse || state.tint != color)
  {
    push_material(state, state.context->whitediffuse, Vec4(0, 0, 1, 1), color);
  }

  auto xbasis = rotate(Vec2(1, 0), rotation);
  auto ybasis = rotate(Vec2(0, 1), rotation);

  push_model(state, (rect.max.x - rect.min.x) * xbasis, (rect.max.y - rect.min.y) * ybasis, position + rect.min.x*xbasis + rect.min.y*ybasis, 0);
}


///////////////////////// SpriteList::push_rect_outline /////////////////////
void SpriteList::push_rect_outline(BuildState &state, Vec2 const &position, Rect2 const &rect, Color4 const &color, float thickness)
{
  push_rect(state, position, Rect2({ rect.min.x - 0.5f*thickness, rect.min.y - 0.5f*thickness }, { rect.max.x + 0.5f*thickness, rect.min.y + 0.5f*thickness }), color);
  push_rect(state, position, Rect2({ rect.min.x - 0.5f*thickness, rect.min.y + 0.5f*thickness }, { rect.min.x + 0.5f*thickness, rect.max.y - 0.5f*thickness }), color);
  push_rect(state, position, Rect2({ rect.max.x - 0.5f*thickness, rect.min.y + 0.5f*thickness }, { rect.max.x + 0.5f*thickness, rect.max.y - 0.5f*thickness }), color);
  push_rect(state, position, Rect2({ rect.max.x + 0.5f*thickness, rect.max.y + 0.5f*thickness }, { rect.min.x - 0.5f*thickness, rect.max.y - 0.5f*thickness }), color);
}


///////////////////////// SpriteList::push_rect_outline /////////////////////
void SpriteList::push_rect_outline(BuildState &state, Vec2 const &position, Rect2 const &rect, float rotation, Color4 const &color, float thickness)
{
  auto a = position + rotate(Vec2(rect.min.x, rect.min.y), rotation);
  auto b = position + rotate(Vec2(rect.max.x, rect.min.y), rotation);
  auto c = position + rotate(Vec2(rect.max.x, rect.max.y), rotation);
  auto d = position + rotate(Vec2(rect.min.x, rect.max.y), rotation);

  push_line(state, a, b, color, thickness);
  push_line(state, b, c, color, thickness);
  push_line(state, c, d, color, thickness);
  push_line(state, d, a, color, thickness);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &xbasis, Vec2 const &ybasis, Vec2 const &position, float size, Sprite const *sprite, Color4 const &tint)
{
  push_sprite(state, xbasis, ybasis, position, size, sprite, 0, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &xbasis, Vec2 const &ybasis, Vec2 const &position, float size, Sprite const *sprite, float layer, Color4 const &tint)
{
  if (!sprite)
    return;

  if (!sprite->ready())
  {
    state.resources->request(*state.platform, sprite);

    if (!sprite->ready())
      return;
  }

  if (state.texture != sprite->atlas->texture || state.texcoords != sprite->extent || state.tint != tint)
  {
    push_material(state, sprite->atlas->texture, sprite->extent, tint);
  }

  auto dim = Vec2(size * sprite->aspect, size);
  auto align = Vec2(sprite->align.x * dim.x, sprite->align.y * dim.y);

  push_model(state, dim.x * xbasis, dim.y * ybasis, position - align.x*xbasis - align.y*ybasis, layer);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, float size, Sprite const *sprite, Color4 const &tint)
{
  auto xbasis = Vec2(1, 0);
  auto ybasis = Vec2(0, 1);

  push_sprite(state, xbasis, ybasis, position, size, sprite, 0, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, float size, Sprite const *sprite, float layer, Color4 const &tint)
{
  auto xbasis = Vec2(1, 0);
  auto ybasis = Vec2(0, 1);

  push_sprite(state, xbasis, ybasis, position, size, sprite, layer, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, float size, float rotation, Sprite const *sprite, Color4 const &tint)
{ 
  auto xbasis = rotate(Vec2(1, 0), rotation);
  auto ybasis = rotate(Vec2(0, 1), rotation);

  push_sprite(state, xbasis, ybasis, position, size, sprite, 0, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, float size, float rotation, Sprite const *sprite, float layer, Color4 const &tint)
{  
  auto xbasis = rotate(Vec2(1, 0), rotation);
  auto ybasis = rotate(Vec2(0, 1), rotation);

  push_sprite(state, xbasis, ybasis, position, size, sprite, layer, tint);
}


///////////////////////// SpriteList::push_text /////////////////////////////
void SpriteList::push_text(BuildState &state, lml::Vec2 const &xbasis, lml::Vec2 const &ybasis, Vec2 const &position, float size, Font const *font, const char *str, Color4 const &tint)
{
  if (!font)
    return;

  if (!font->ready())
  {
    state.resources->request(*state.platform, font);

    if (!font->ready())
      return;
  }

  auto scale = size / font->height();

  auto cursor = position;

  uint32_t othercodepoint = 0;

  for(const char *ch = str; *ch; ++ch)
  {
    uint32_t codepoint = *ch;

    if (codepoint >= (size_t)font->glyphcount)
      codepoint = 0;

    cursor += scale * font->width(othercodepoint, codepoint) * xbasis;

    push_material(state, font->glyphs->texture, font->texcoords[codepoint], tint);

    push_model(state, scale * font->dimension[codepoint].x * xbasis, scale * font->dimension[codepoint].y * ybasis, cursor - scale * font->alignment[codepoint].x*xbasis - scale * font->alignment[codepoint].y*ybasis, 0);

    othercodepoint = codepoint;
  }
}


///////////////////////// SpriteList::push_text /////////////////////////////
void SpriteList::push_text(BuildState &state, Vec2 const &position, float size, Font const *font, const char *str, Color4 const &tint)
{
  auto xbasis = Vec2(1, 0);
  auto ybasis = Vec2(0, 1);

  push_text(state, xbasis, ybasis, position, size, font, str, tint);
}


///////////////////////// SpriteList::push_text /////////////////////////////
void SpriteList::push_text(BuildState &state, Vec2 const &position, float size, float rotation, Font const *font, const char *str, Color4 const &tint)
{
  auto xbasis = rotate(Vec2(1, 0), rotation);
  auto ybasis = rotate(Vec2(0, 1), rotation);

  push_text(state, xbasis, ybasis, position, size, font, str, tint);
}


///////////////////////// SpriteList::push_scissor //////////////////////////
void SpriteList::push_scissor(BuildState &state, Rect2 const &cliprect)
{
  VkRect2D scissor = {};
  scissor.offset.x = cliprect.min.x;
  scissor.offset.y = cliprect.min.y;
  scissor.extent.width = cliprect.max.x - cliprect.min.x;
  scissor.extent.height = cliprect.max.y - cliprect.min.y;

  vkCmdSetScissor(*state.commandlist, 0, 1, &scissor);
}


///////////////////////// SpriteList::finalise //////////////////////////////
void SpriteList::finalise(BuildState &state)
{
  assert(state.commandlist);

  auto &commandlist = *state.commandlist;

  commandlist.release(state.modelset);
  commandlist.release(state.materialset);

  state.commandlist->end();

  state.commandlist = nullptr;

  state.resources->assets()->release_barrier(state.assetbarrier);
}
