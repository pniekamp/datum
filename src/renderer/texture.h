//
// Datum - texture
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resource.h"

//|---------------------- Texture -------------------------------------------
//|--------------------------------------------------------------------------

class Texture
{
  public:

    enum class Format
    {
      RGBA,
      SRGBA,
      RGBM,
      RGBE,
      FLOAT16,
      FLOAT32
    };

  public:
    friend Texture const *ResourceManager::create<Texture>(Asset const *asset, Format format);
    friend Texture const *ResourceManager::create<Texture>(int width, int height, int layers, int levels, Format format);

    friend void ResourceManager::update<Texture>(Texture const *texture, ResourceManager::TransferLump const *lump);

    bool ready() const { return (state == State::Ready); }

    int width;
    int height;
    int layers;
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
    Texture() = default;
};

