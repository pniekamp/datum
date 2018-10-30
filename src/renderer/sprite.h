//
// Datum - sprite
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "texture.h"

//|---------------------- Sprite --------------------------------------------
//|--------------------------------------------------------------------------

class Sprite
{
  public:
    friend Sprite const *ResourceManager::create<Sprite>(Asset const *asset);
    friend Sprite const *ResourceManager::create<Sprite>(Asset const *asset, lml::Vec2 pivot);
    friend Sprite const *ResourceManager::create<Sprite>(Texture const *atlas, lml::Rect2 region, lml::Vec2 pivot);

    bool ready() const { return (state == State::Ready); }

    int flags;

    int width;
    int height;
    int layers;
    float aspect;
    lml::Vec2 pivot;
    lml::Vec4 extent;

    Texture const *atlas;

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
    Sprite() = default;
};

