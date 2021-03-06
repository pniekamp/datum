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

enum ShaderLocation
{
  sceneset = 0,
  materialset = 1,
  modelset = 2,
  extendedset = 3,

  albedomap = 1,
};

struct MaterialSet
{
  alignas(16) Color4 color;
};

struct ModelSet
{
  alignas( 8) Vec2 xbasis;
  alignas( 8) Vec2 ybasis;
  alignas(16) Vec4 position;
  alignas(16) Vec4 texcoords;
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

  state.clipx = 0;
  state.clipy = 0;
  state.clipwidth = context.width;
  state.clipheight = context.height;

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

  begin(context.vulkan, spritecommands, context.framebuffer, context.overlaypass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bind_descriptor(spritecommands, context.pipelinelayout, ShaderLocation::sceneset, context.scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_pipeline(spritecommands, context.spritepipeline, 0, 0, context.width, context.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bind_vertexbuffer(spritecommands, 0, context.unitquad);

  m_commandlump = { resources, commandlump };

  state.commandlump = commandlump;

  return true;
}


///////////////////////// SpriteList::push_material /////////////////////////
void SpriteList::push_material(BuildState &state, Vulkan::Texture const &texture, Color4 const &tint)
{
  assert(state.commandlump);

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.materialset.available() < sizeof(MaterialSet) || state.image != texture.image)
  {
    state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, texture);

      state.image = texture.image;
    }
  }

  if (state.materialset)
  {
    auto offset = state.materialset.reserve(sizeof(MaterialSet));

    auto materialset = state.materialset.memory<MaterialSet>(offset);

    materialset->color = tint;

    bind_descriptor(spritecommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.color = tint;
  }
}


///////////////////////// SpriteList::push_model ////////////////////////////
void SpriteList::push_model(BuildState &state, Vec2 const &position, Vec2 const &xbasis, Vec2 const &ybasis, Vec4 const &extent, float layer0, float layer1)
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
    modelset->position = Vec4(position, layer0, layer1);
    modelset->texcoords = extent;

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
  if (state.image != state.context->whitediffuse || state.color != color)
  {
    push_material(state, state.context->whitediffuse, color);
  }

  auto xbasis = Vec2(1, 0);
  auto ybasis = Vec2(0, 1);

  push_model(state, position + rect.min.x*xbasis + rect.min.y*ybasis, (rect.max.x - rect.min.x) * xbasis, (rect.max.y - rect.min.y) * ybasis, Vec4(0, 0, 1, 1), 0, 0);
}


///////////////////////// SpriteList::push_rect /////////////////////////////
void SpriteList::push_rect(BuildState &state, Vec2 const &position, Rect2 const &rect, float rotation, Color4 const &color)
{
  if (state.image != state.context->whitediffuse || state.color != color)
  {
    push_material(state, state.context->whitediffuse, color);
  }

  auto xbasis = rotate(Vec2(1, 0), rotation);
  auto ybasis = rotate(Vec2(0, 1), rotation);

  push_model(state, position + rect.min.x*xbasis + rect.min.y*ybasis, (rect.max.x - rect.min.x) * xbasis, (rect.max.y - rect.min.y) * ybasis, Vec4(0, 0, 1, 1), 0, 0);
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
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, Vec2 const &xbasis, Vec2 const &ybasis, Sprite const *sprite, Color4 const &tint)
{
  push_sprite(state, position, xbasis, ybasis, sprite, 0, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, Vec2 const &xbasis, Vec2 const &ybasis, Sprite const *sprite, float layer, Color4 const &tint)
{
  assert(sprite && sprite->ready());

  if (state.image != sprite->atlas->texture || state.color != tint)
  {
    push_material(state, sprite->atlas->texture, tint);
  }

  push_model(state, position - sprite->pivot.x*xbasis - sprite->pivot.y*ybasis, xbasis, ybasis, sprite->extent, layer, fmod(floor(layer) + 1, (float)sprite->layers));
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, Vec2 const &xbasis, Vec2 const &ybasis, Sprite const *sprite, Rect2 const &region, Color4 const &tint)
{
  push_sprite(state, position, xbasis, ybasis, sprite, region, 0, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, Vec2 const &xbasis, Vec2 const &ybasis, Sprite const *sprite, Rect2 const &region, float layer, Color4 const &tint)
{
  assert(sprite && sprite->ready());

  if (state.image != sprite->atlas->texture || state.color != tint)
  {
    push_material(state, sprite->atlas->texture, tint);
  }

  Vec4 extent;
  extent.x = sprite->extent.x + (region.min.x * sprite->extent.z);
  extent.y = sprite->extent.y + (region.min.y * sprite->extent.w);
  extent.z = sprite->extent.z * (region.max.x - region.min.x);
  extent.w = sprite->extent.w * (region.max.y - region.min.y);

  push_model(state, position - sprite->pivot.x*xbasis - sprite->pivot.y*ybasis, xbasis, ybasis, extent, layer, fmod(floor(layer) + 1, (float)sprite->layers));
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, float size, Sprite const *sprite, Color4 const &tint)
{
  auto xbasis = Vec2(size * sprite->aspect, 0);
  auto ybasis = Vec2(0, size);

  push_sprite(state, position, xbasis, ybasis, sprite, 0, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, float size, Sprite const *sprite, float layer, Color4 const &tint)
{
  auto xbasis = Vec2(size * sprite->aspect, 0);
  auto ybasis = Vec2(0, size);

  push_sprite(state, position, xbasis, ybasis, sprite, layer, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, float size, float rotation, Sprite const *sprite, Color4 const &tint)
{ 
  auto xbasis = rotate(Vec2(size * sprite->aspect, 0), rotation);
  auto ybasis = rotate(Vec2(0, size), rotation);

  push_sprite(state, position, xbasis, ybasis, sprite, 0, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, float size, float rotation, Sprite const *sprite, float layer, Color4 const &tint)
{  
  auto xbasis = rotate(Vec2(size * sprite->aspect, 0), rotation);
  auto ybasis = rotate(Vec2(0, size), rotation);

  push_sprite(state, position, xbasis, ybasis, sprite, layer, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, Rect2 const &rect, Sprite const *sprite, Color4 const &tint)
{
  auto xbasis = Vec2(1, 0);
  auto ybasis = Vec2(0, 1);

  push_sprite(state, position + rect.min.x*xbasis + rect.min.y*ybasis, (rect.max.x - rect.min.x) * xbasis, (rect.max.y - rect.min.y) * ybasis, sprite, 0, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, Rect2 const &rect, Sprite const *sprite, float layer, Color4 const &tint)
{
  auto xbasis = Vec2(1, 0);
  auto ybasis = Vec2(0, 1);

  push_sprite(state, position + rect.min.x*xbasis + rect.min.y*ybasis, (rect.max.x - rect.min.x) * xbasis, (rect.max.y - rect.min.y) * ybasis, sprite, layer, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, Rect2 const &rect, float rotation, Sprite const *sprite, Color4 const &tint)
{
  auto xbasis = rotate(Vec2(1, 0), rotation);
  auto ybasis = rotate(Vec2(0, 1), rotation);

  push_sprite(state, position + rect.min.x*xbasis + rect.min.y*ybasis, (rect.max.x - rect.min.x) * xbasis, (rect.max.y - rect.min.y) * ybasis, sprite, 0, tint);
}


///////////////////////// SpriteList::push_sprite ///////////////////////////
void SpriteList::push_sprite(BuildState &state, Vec2 const &position, Rect2 const &rect, float rotation, Sprite const *sprite, float layer, Color4 const &tint)
{
  auto xbasis = rotate(Vec2(1, 0), rotation);
  auto ybasis = rotate(Vec2(0, 1), rotation);

  push_sprite(state, position + rect.min.x*xbasis + rect.min.y*ybasis, (rect.max.x - rect.min.x) * xbasis, (rect.max.y - rect.min.y) * ybasis, sprite, layer, tint);
}


///////////////////////// SpriteList::push_text /////////////////////////////
void SpriteList::push_text(BuildState &state, Vec2 const &position, Vec2 const &xbasis, Vec2 const &ybasis, Font const *font, const char *str, Color4 const &tint)
{
  assert(font && font->ready());

  if (state.image != font->sheet->texture || state.color != tint)
  {
    push_material(state, font->sheet->texture, tint);
  }

  auto cursor = position;

  uint32_t codepoint;;
  uint32_t lastcodepoint = 0;

  for(auto ch = (uint8_t const *)str; *ch; ++ch)
  {
    if (*ch >= font->glyphcount)
      continue;

    codepoint = *ch;

    cursor += font->width(lastcodepoint, codepoint) * xbasis;

    push_model(state, cursor - font->alignment[codepoint].x * xbasis - font->alignment[codepoint].y * ybasis, font->dimension[codepoint].x * xbasis, font->dimension[codepoint].y * ybasis, font->texcoords[codepoint], 0, 0);

    lastcodepoint = codepoint;
  }
}


///////////////////////// SpriteList::push_text /////////////////////////////
void SpriteList::push_text(BuildState &state, Vec2 const &position, float size, Font const *font, const char *str, Color4 const &tint)
{
  auto xbasis = Vec2(size / font->height(), 0);
  auto ybasis = Vec2(0, size / font->height());

  push_text(state, position, xbasis, ybasis, font, str, tint);
}


///////////////////////// SpriteList::push_text /////////////////////////////
void SpriteList::push_text(BuildState &state, Vec2 const &position, float size, float rotation, Font const *font, const char *str, Color4 const &tint)
{
  auto xbasis = rotate(Vec2(size / font->height(), 0), rotation);
  auto ybasis = rotate(Vec2(0, size / font->height()), rotation);

  push_text(state, position, xbasis, ybasis, font, str, tint);
}


///////////////////////// SpriteList::push_texture //////////////////////////
void SpriteList::push_texture(BuildState &state, Vec2 const &position, Rect2 const &rect, Vulkan::Texture const &texture, float layer, Color4 const &tint)
{
  auto xbasis = Vec2(1, 0);
  auto ybasis = Vec2(0, 1);

  push_material(state, texture, tint);

  push_model(state, position + rect.min.x*xbasis + rect.min.y*ybasis, (rect.max.x - rect.min.x) * xbasis, (rect.max.y - rect.min.y) * ybasis, Vec4(0, 0, 1, 1), layer, layer);
}


///////////////////////// SpriteList::push_scissor //////////////////////////
void SpriteList::push_scissor(BuildState &state, Rect2 const &cliprect)
{
  assert(state.commandlump);

  state.clipx = (int)(cliprect.min.x);
  state.clipy = (int)(cliprect.min.y);
  state.clipwidth = (int)(cliprect.max.x - cliprect.min.x);
  state.clipheight = (int)(cliprect.max.y - cliprect.min.y);

  scissor(spritecommands, state.clipx, state.clipy, state.clipwidth, state.clipheight);
}


///////////////////////// SpriteList::finalise //////////////////////////////
void SpriteList::finalise(BuildState &state)
{
  assert(state.commandlump);

  auto &context = *state.context;

  end(context.vulkan, spritecommands);

  state.commandlump = nullptr;
}
