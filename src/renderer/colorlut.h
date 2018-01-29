//
// Datum - colorlut
//

//
// Copyright (c) 2018 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "texture.h"

//|---------------------- ColorLut ------------------------------------------
//|--------------------------------------------------------------------------

class ColorLut
{
  public:

    enum class Format
    {
      RGBA,
      RGBE
    };

  public:
    friend ColorLut const *ResourceManager::create<ColorLut>(Asset const *asset, Format format);

    bool ready() const { return (state == State::Ready); }

    int width;
    int height;
    int depth;
    Format format;

    Vulkan::Texture texture;

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
    ResourceManager::TransferLump const *transferlump;

    std::atomic<State> state;

  protected:
    ColorLut() = default;
};
