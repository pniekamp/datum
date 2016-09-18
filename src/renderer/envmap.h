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
    friend EnvMap const *ResourceManager::create<EnvMap>(Asset const *asset);
    friend EnvMap const *ResourceManager::create<EnvMap>(int width, int height);

    bool ready() const { return (state == State::Ready); }

    int width;
    int height;

    Vulkan::Texture texture;

  public:

    enum class State
    {
      Empty,
      Loading,
      Ready,
    };

    std::atomic<State> state;

  private:
    EnvMap() = default;
};
