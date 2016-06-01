//
// Datum - material
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "texture.h"


//|---------------------- Material ------------------------------------------
//|--------------------------------------------------------------------------

class Material
{
  using Color3 = lml::Color3;

  public:
    friend Material const *ResourceManager::create<Material>(Asset const *asset);
    friend Material const *ResourceManager::create<Material>(lml::Color3 color, float metalness, float smoothness);
    friend Material const *ResourceManager::create<Material>(lml::Color3 color, float metalness, float smoothness, float reflectivity, Texture const *albedomap, Texture const *specularmap, Texture const *normalmap);

    bool ready() const { return (state == State::Ready); }

    int flags;

    Color3 color;

    float metalness;
    float smoothness;
    float reflectivity;

    Texture const *albedomap;
    Texture const *specularmap;
    Texture const *normalmap;

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

  private:
    Material() = default;
};

