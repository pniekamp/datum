//
// Datum - skybox
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "texture.h"


//|---------------------- SkyBox --------------------------------------------
//|--------------------------------------------------------------------------

class SkyBox
{
  public:
    friend SkyBox const *ResourceManager::create<SkyBox>(Asset const *asset);

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
    SkyBox() = default;
};
