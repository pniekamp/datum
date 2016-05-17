//
// Datum - skybox
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "texture.h"


//|---------------------- Skybox --------------------------------------------
//|--------------------------------------------------------------------------

class Skybox
{
  public:
    friend Skybox const *ResourceManager::create<Skybox>(Asset const *asset);

    bool ready() const { return (state == State::Ready); }

    Vulkan::Texture cubemap;

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
    Skybox() = default;
};
