//
// Datum - font
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "texture.h"

//|---------------------- Font ----------------------------------------------
//|--------------------------------------------------------------------------

class Font
{
  public:
    friend Font const *ResourceManager::create<Font>(Asset const *asset);

    bool ready() const { return (state == State::Ready); }

    int ascent;
    int descent;
    int leading;

    int width(const char *str) const;
    int width(uint32_t codepoint, uint32_t nextcodepoint) const;
    int height() const;
    int lineheight() const;

    int glyphcount;
    lml::Vec4 const *texcoords;//[count]
    lml::Vec2 const *alignment;//[count]
    lml::Vec2 const *dimension;//[count]
    uint8_t const *advance;//[count][count];

    Texture const *sheet;

  public:

    enum class State
    {
      Empty,
      Loading,
      Waiting,
      Testing,
      Ready,
    };

    Asset const *asset;

    std::atomic<State> state;

    alignas(16) uint8_t data[1];

  protected:
    Font() = default;
};


///////////////////////// Font::width ///////////////////////////////////////
inline int Font::width(const char *str) const
{
  int sum = 0;

  uint32_t codepoint;
  uint32_t lastcodepoint = 0;

  for(uint8_t const *ch = (uint8_t const *)str; *ch; ++ch)
  {
    if (*ch >= glyphcount)
      continue;

    codepoint = *ch;

    sum += width(lastcodepoint, codepoint);

    lastcodepoint = codepoint;
  }

  sum += width(lastcodepoint, 0);

  return sum;
}


///////////////////////// Font::width ///////////////////////////////////////
inline int Font::width(uint32_t codepoint, uint32_t nextcodepoint) const
{
  return advance[codepoint*glyphcount + nextcodepoint];
}


///////////////////////// Font::height //////////////////////////////////////
inline int Font::height() const
{
  return ascent + descent;
}


///////////////////////// Font::lineheight //////////////////////////////////
inline int Font::lineheight() const
{
  return ascent + descent + leading;
}
