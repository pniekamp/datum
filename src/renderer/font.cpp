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

namespace
{
  size_t font_datasize(int glyphcount)
  {
    return 8*glyphcount*sizeof(float) + glyphcount*glyphcount*sizeof(uint8_t);
  }
}

//|---------------------- Font ----------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
Font const *ResourceManager::create<Font>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(Font) + font_datasize(asset->glyphcount));

  if (!slot)
    return nullptr;

  auto font = new(slot) Font;

  font->ascent = asset->ascent;
  font->descent = asset->descent;
  font->leading = asset->leading;
  font->glyphcount = asset->glyphcount;
  font->glyphs = nullptr;
  font->texcoords = nullptr;
  font->alignment = nullptr;
  font->dimension = nullptr;
  font->advance = nullptr;
  font->asset = asset;
  font->state = Font::State::Empty;

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
    if (auto asset = slot->asset)
    {
      assert(assets()->barriercount != 0);

      if (auto bits = m_assets->request(platform, asset))
      {       
        auto payload = reinterpret_cast<PackFontPayload const *>(bits);

        auto count = asset->glyphcount;
        auto glyphs = payload->glyphatlas;
        auto xtable = PackFontPayload::xtable(payload, asset->glyphcount);
        auto ytable = PackFontPayload::ytable(payload, asset->glyphcount);
        auto widthtable = PackFontPayload::widthtable(payload, asset->glyphcount);
        auto heighttable = PackFontPayload::heighttable(payload, asset->glyphcount);
        auto offsetxtable = PackFontPayload::offsetxtable(payload, asset->glyphcount);
        auto offsetytable = PackFontPayload::offsetytable(payload, asset->glyphcount);
        auto advancetable = PackFontPayload::advancetable(payload, asset->glyphcount);

        slot->glyphs = create<Texture>(assets()->find(asset->id + glyphs), Texture::Format::RGBA);

        float sx = (slot->glyphs) ? 1.0f / slot->glyphs->width : 0.0f;
        float sy = (slot->glyphs) ? 1.0f / slot->glyphs->height : 0.0f;

        auto texcoordsdata = reinterpret_cast<Vec4*>(slot->data + 0*count*sizeof(float));

        for(int codepoint = 0; codepoint < count; ++codepoint)
          texcoordsdata[codepoint] = Vec4(sx * xtable[codepoint], sy * ytable[codepoint], sx * widthtable[codepoint], sy * heighttable[codepoint]);

        slot->texcoords = texcoordsdata;

        auto alignmentdata = reinterpret_cast<Vec2*>(slot->data + 4*count*sizeof(float));

        for(int codepoint = 0; codepoint < count; ++codepoint)
          alignmentdata[codepoint] = Vec2(offsetxtable[codepoint], offsetytable[codepoint]);

        slot->alignment = alignmentdata;

        auto dimensiondata = reinterpret_cast<Vec2*>(slot->data + 6*count*sizeof(float));

        for(int codepoint = 0; codepoint < count; ++codepoint)
          dimensiondata[codepoint] = Vec2(widthtable[codepoint], heighttable[codepoint]);

        slot->dimension = dimensiondata;

        auto advancedata = reinterpret_cast<uint8_t*>(slot->data + 8*count*sizeof(float));

        memcpy(advancedata, advancetable, count*count*sizeof(uint8_t));

        slot->advance = advancedata;
      }
    }

    slot->state = (slot->texcoords && slot->alignment && slot->dimension && slot->advance && slot->glyphs) ? Font::State::Waiting : Font::State::Empty;
  }

  Font::State waiting = Font::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, Font::State::Testing))
  {
    request(platform, slot->glyphs);

    slot->state = (slot->glyphs->ready()) ? Font::State::Ready : Font::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Font>(Font const *font)
{
  defer_destroy(font);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Font>(Font const *font)
{
  if (font)
  {
    if (font->glyphs)
      destroy(font->glyphs);

    font->~Font();

    release_slot(const_cast<Font*>(font), sizeof(Font) + font_datasize(font->glyphcount));
  }
}
