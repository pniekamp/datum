//
// Datum - texture
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "renderer.h"


//|---------------------- Texture -------------------------------------------
//|--------------------------------------------------------------------------

class Texture
{
  public:

    enum class Format
    {
      RGBA,
      SRGBA,
    };

  public:
    friend Texture const *ResourceManager::create<Texture>(Asset const *asset, Format format);
    friend Texture const *ResourceManager::create<Texture>(int width, int height, Format format);

    friend void ResourceManager::update<Texture>(Texture const *texture);
    friend void ResourceManager::update<Texture>(Texture const *texture, void const *bits);

    bool ready() const { return (state == State::Ready); }

    int width;
    int height;
    int layers;
    Format format;

    uint32_t *memory;

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

    std::atomic<State> state;

    ResourceManager::TransferLump const *transferlump;

  private:
    Texture() = default;
};

