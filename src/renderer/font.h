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

    int width(uint32_t codepoint, uint32_t nextcodepoint) const;
    int height() const;
    int lineheight() const;

    int glyphcount;
    lml::Vec4 *texcoords;//[count]
    lml::Vec2 *alignment;//[count]
    lml::Vec2 *dimension;//[count]
    uint8_t *advance;//[count][count];

    Texture const *glyphs;

  public:

    enum class State
    {
      Empty,
      Loading,
      Waiting,
      Testing,
      Ready,
    };

    std::atomic<State> state;

    void *memory;
    size_t memorysize;

  private:
    Font() = default;
};


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
