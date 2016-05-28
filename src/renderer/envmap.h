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

    bool ready() const { return (state == State::Ready); }

    Vulkan::Texture envmap;

  public:

    Texture const *texture;

    enum class State
    {
      Loading,
      Finalising,
      Ready,
    };

    std::atomic<State> state;

  private:
    EnvMap() = default;
};
