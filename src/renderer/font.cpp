//
// Datum - font
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "font.h"
#include "resource.h"
#include "debug.h"

using namespace std;
using namespace lml;


//|---------------------- Font ----------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
Font const *ResourceManager::create<Font>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(Font));

  if (!slot)
    return nullptr;

  auto font = new(slot) Font;

  font->ascent = asset->ascent;
  font->descent = asset->descent;
  font->leading = asset->leading;

  font->glyphcount = asset->glyphcount;

  font->glyphs = nullptr;

  font->memory = nullptr;

  set_slothandle(slot, asset);

  return font;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<Font>(DatumPlatform::PlatformInterface &platform, Font const *font)
{
  assert(font);

  auto slot = const_cast<Font*>(font);

  Font::State empty = Font::State::Empty;

  if (slot->state.compare_exchange_strong(empty, Font::State::Loading))
  {
    auto asset = get_slothandle<Asset const *>(slot);

    auto bits = m_assets->request(platform, asset);

    if (bits)
    {
      auto count = (size_t)asset->glyphcount;
      auto glyphatlas = reinterpret_cast<uint32_t const*>((size_t)bits);
      auto xtable = reinterpret_cast<uint16_t*>((size_t)bits + sizeof(uint32_t) + 0 * count * sizeof(uint16_t));
      auto ytable = reinterpret_cast<uint16_t*>((size_t)bits + sizeof(uint32_t) + 1 * count * sizeof(uint16_t));
      auto widthtable = reinterpret_cast<uint16_t*>((size_t)bits + sizeof(uint32_t) + 2 * count * sizeof(uint16_t));
      auto heighttable = reinterpret_cast<uint16_t*>((size_t)bits + sizeof(uint32_t) + 3 * count * sizeof(uint16_t));
      auto offsetxtable = reinterpret_cast<uint16_t*>((size_t)bits + sizeof(uint32_t) + 4 * count * sizeof(uint16_t));
      auto offsetytable = reinterpret_cast<uint16_t*>((size_t)bits + sizeof(uint32_t) + 5 * count * sizeof(uint16_t));
      auto advancetable = reinterpret_cast<uint8_t const*>((size_t)bits + sizeof(uint32_t) + 6 * count * sizeof(uint16_t));

      slot->memorysize = 8*count*sizeof(float) + count*count*sizeof(uint8_t);

      slot->memory = acquire_slot(font->memorysize);

      if (slot->memory)
      {
        slot->glyphs = create<Texture>(assets()->find(asset->id + *glyphatlas), Texture::Format::RGBA);

        float sx = 1.0f / font->glyphs->width;
        float sy = 1.0f / font->glyphs->height;

        slot->texcoords = reinterpret_cast<Vec4*>((size_t)slot->memory + 0*count*sizeof(float));

        for(size_t codepoint = 0; codepoint < count; ++codepoint)
          slot->texcoords[codepoint] = Vec4(sx * xtable[codepoint], sy * ytable[codepoint], sx * widthtable[codepoint], sy * heighttable[codepoint]);

        slot->alignment = reinterpret_cast<Vec2*>((size_t)slot->memory + 4*count*sizeof(float));

        for(size_t codepoint = 0; codepoint < count; ++codepoint)
          slot->alignment[codepoint] = Vec2(offsetxtable[codepoint], offsetytable[codepoint]);

        slot->dimension = reinterpret_cast<Vec2*>((size_t)slot->memory + 6*count*sizeof(float));

        for(size_t codepoint = 0; codepoint < count; ++codepoint)
          slot->dimension[codepoint] = Vec2(widthtable[codepoint], heighttable[codepoint]);

        slot->advance = reinterpret_cast<uint8_t*>((size_t)slot->memory + 8*count*sizeof(float));

        memcpy(slot->advance, advancetable, count*count*sizeof(uint8_t));

        slot->state = Font::State::Waiting;
      }
      else
        slot->state = Font::State::Empty;
    }
    else
      slot->state = Font::State::Empty;
  }

  Font::State waiting = Font::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, Font::State::Testing))
  {
    request(platform, slot->glyphs);

    if (slot->glyphs->ready())
    {
      slot->state = Font::State::Ready;
    }
    else
      slot->state = Font::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Font>(Font const *font)
{
  assert(font);

  defer_destroy(font);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Font>(Font const *font)
{
  assert(font);

  auto slot = const_cast<Font*>(font);

  if (font->glyphs)
    destroy(font->glyphs);

  if (slot->memory)
    release_slot(slot->memory, slot->memorysize);

  font->~Font();

  release_slot(slot, sizeof(Font));
}
