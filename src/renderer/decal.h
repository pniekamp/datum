//
// Datum - decal
//

//
// Copyright (c) 2017 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "material.h"

//|---------------------- Decal --------------------------------------------
//|--------------------------------------------------------------------------

class Decal
{
  public:
    friend Decal const *ResourceManager::create<Decal>(Asset const *asset);
    friend Decal const *ResourceManager::create<Decal>(Material const *material, lml::Rect2 extent);

    bool ready() const { return (state == State::Ready); }

    int flags;

    lml::Vec4 extent;

    Material const *material;

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

  protected:
    Decal() = default;
};

