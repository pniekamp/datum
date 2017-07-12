//
// Datum - sprite list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "spritelist.h"
#include <leap/lml/matrix.h>
#include <leap/lml/matrixconstants.h>
#include "debug.h"

using namespace std;
using namespace lml;
using namespace Vulkan;
using namespace DatumPlatform;
using leap::alignto;
using leap::extentof;

enum ShaderLocation
{
  sceneset = 0,
  materialset = 1,
  modelset = 2,

  albedomap = 1,
};

struct SceneSet
{
  alignas(16) Matrix4f worldview;
};

struct SpriteMaterialSet
{
  alignas(16) Color4 color;
  alignas(16) Vec4 texcoords;
};

struct ModelSet
{
  alignas( 8) Vec2 xbasis;
  alignas( 8) Vec2 ybasis;
  alignas(16) Vec4 position;
};


///////////////////////// draw_sprites //////////////////////////////////////
void draw_sprites(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Sprites const &sprites)
{
  execute(commandbuffer, sprites.spritecommands);
}


//|---------------------- SpriteList ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// SpriteList::begin /////////////////////////////////
bool SpriteList::begin(BuildState &state, RenderContext &context, ResourceManager &resources)
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

  spritecommands = commandlump->allocate_commandbuffer();

  if (!spritecommands)
  {
    resources.destroy(commandlump);
    return false;
  }

  using Vulkan::begin;

  begin(context.vulkan, spritecommands, 0, context.overlaypass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bind_pipeline(spritecommands, context.spritepipeline, 0, 0, context.width, context.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(spritecommands, 0, context.unitquad);

  m_commandlump = { resources, commandlump };

  state.commandlump = commandlump;

  return true;
}


///////////////////////// SpriteList::viewport //////////////////////////////
void SpriteList::viewport(BuildState &state, Rect2 const &viewport) const
{
  assert(state.commandlump);

  auto &context = *state.context;

  auto worldview = OrthographicProjection(viewport.min.x, viewport.min.y, viewport.max.x, viewport.max.y, 0.0f, 1000.0f);

  push(spritecommands, context.pipelinelayout, 0, sizeof(worldview), &worldview, VK_SHADER_STAGE_VERTEX_BIT);
}


///////////////////////// SpriteList::viewport //////////////////////////////
void SpriteList::viewport(BuildState &state, DatumPlatform::Viewport const &viewport) const
{
  assert(state.commandlump);

  auto &context = *state.context;

  auto worldview = OrthographicProjection(0.0f, 0.0f, (float)viewport.width, (float)viewport.height, 0.0f, 1000.0f);

  push(spritecommands, context.pipelinelayout, 0, sizeof(worldview), &worldview, VK_SHADER_STAGE_VERTEX_BIT);
}


///////////////////////// SpriteList::push_material /////////////////////////
void SpriteList::push_material(BuildState &state, Vulkan::Texture const &texture, Vec4 const &texcoords, Color4 const &tint)
{
  assert(state.commandlump);

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.materialset.available() < sizeof(SpriteMaterialSet) || state.texture != texture)
  {
    state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(SpriteMaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, texture);

      state.texture = texture;
    }
  }

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(SpriteMaterialSet));

    auto materialset = state.materialset.memory<SpriteMaterialSet>(offset);

    materialset->color = premultiply(tint);
    materialset->texcoords = texcoords;

    bind_descriptor(spritecommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.color = tint;
    state.texcoords = texcoords;
  }
}


///////////////////////// SpriteList::push_model ////////////////////////////
void SpriteList::push_model(SpriteList::BuildState &state, Vec2 xbasis, Vec2 ybasis, Vec2 position, float layer)
{
  assert(state.commandlump);

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.modelset.available() < sizeof(ModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(ModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->xbasis = xbasis;
    modelset->ybasis = ybasis;
    modelset->position = Vec4(position, layer - 0.5f + 1e-3f, 1);

    bind_descriptor(spritecommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(spritecommands, context.unitquad.vertexcount, 1, 0, 0);
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
  if (state.texture != state.context->whitediffuse || state.color != color)
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
  if (state.texture != state.context->whitediffuse || state.color != color)
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
  assert(sprite && sprite->ready());

  if (state.texture != sprite->atlas->texture || state.texcoords != sprite->extent || state.color != tint)
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
  assert(font && font->ready());

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


///////////////////////// SpriteList::push_texture //////////////////////////
void SpriteList::push_texture(BuildState &state, Vec2 const &position, Rect2 const &rect, Vulkan::Texture const &texture, float layer, lml::Color4 const &tint)
{
  auto xbasis = Vec2(1, 0);
  auto ybasis = Vec2(0, 1);

  push_material(state, texture, Vec4(0, 0, 1, 1), tint);

  push_model(state, (rect.max.x - rect.min.x) * xbasis, (rect.max.y - rect.min.y) * ybasis, position + rect.min.x*xbasis + rect.min.y*ybasis, layer);
}


///////////////////////// SpriteList::push_scissor //////////////////////////
void SpriteList::push_scissor(BuildState &state, Rect2 const &cliprect)
{
  assert(state.commandlump);

  scissor(spritecommands, (int)cliprect.min.x, (int)cliprect.min.y, (int)(cliprect.max.x - cliprect.min.x), (int)(cliprect.max.y - cliprect.min.y));
}


///////////////////////// SpriteList::finalise //////////////////////////////
void SpriteList::finalise(BuildState &state)
{
  assert(state.commandlump);

  auto &context = *state.context;

  end(context.vulkan, spritecommands);

  state.commandlump = nullptr;
}
