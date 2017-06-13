//
// Datum - envmap
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "texture.h"

//|---------------------- EnvMap --------------------------------------------
//|--------------------------------------------------------------------------

class EnvMap
{
  public:

    enum class Format
    {
      RGBE,
      FLOAT16,
      FLOAT32
    };

  public:
    friend EnvMap const *ResourceManager::create<EnvMap>(Asset const *asset);
    friend EnvMap const *ResourceManager::create<EnvMap>(int width, int height, Format format);

    friend void ResourceManager::update<EnvMap>(EnvMap const *envmap, ResourceManager::TransferLump const *lump);

    bool ready() const { return (state == State::Ready); }

    int width;
    int height;
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
    EnvMap() = default;
};
