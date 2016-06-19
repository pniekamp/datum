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
    friend Material const *ResourceManager::create<Material>(lml::Color3 color, float emissive);
    friend Material const *ResourceManager::create<Material>(lml::Color3 color, float metalness, float roughness);
    friend Material const *ResourceManager::create<Material>(lml::Color3 color, float metalness, float roughness, float reflectivity);
    friend Material const *ResourceManager::create<Material>(lml::Color3 color, float metalness, float roughness, float reflectivity, float emissive);
    friend Material const *ResourceManager::create<Material>(lml::Color3 color, float metalness, float roughness, float reflectivity, float emissive, Texture const *albedomap, Texture const *specularmap, Texture const *normalmap);

    friend void ResourceManager::update<Material>(Material const *material, lml::Color3 color, float metalness, float roughness, float reflectivity, float emissive);

    bool ready() const { return (state == State::Ready); }

    int flags;

    Color3 color;

    float metalness;
    float roughness;
    float reflectivity;
    float emissive;

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

