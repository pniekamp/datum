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
  using Color4 = lml::Color4;

  public:
    friend Material const *ResourceManager::create<Material>(Asset const *asset);
    friend Material const *ResourceManager::create<Material>(lml::Color4 color);
    friend Material const *ResourceManager::create<Material>(lml::Color4 color, float emissive);
    friend Material const *ResourceManager::create<Material>(lml::Color4 color, float metalness, float roughness);
    friend Material const *ResourceManager::create<Material>(lml::Color4 color, float metalness, float roughness, float reflectivity);
    friend Material const *ResourceManager::create<Material>(lml::Color4 color, float metalness, float roughness, float reflectivity, float emissive);
    friend Material const *ResourceManager::create<Material>(lml::Color4 color, float metalness, float roughness, float reflectivity, float emissive, Texture const *albedomap, Texture const *normalmap);
    friend Material const *ResourceManager::create<Material>(lml::Color4 color, float metalness, float roughness, float reflectivity, float emissive, Texture const *albedomap, Texture const *surfacemap, Texture const *normalmap);

    friend void ResourceManager::update<Material>(Material const *material, lml::Color4 color);
    friend void ResourceManager::update<Material>(Material const *material, lml::Color4 color, float metalness, float roughness, float reflectivity, float emissive);
    friend void ResourceManager::update<Material>(Material const *material, lml::Color4 color, float metalness, float roughness, float reflectivity, float emissive, Texture const *albedomap, Texture const *surfacemap, Texture const *normalmap);

    bool ready() const { return (state == State::Ready); }

    int flags;

    Color4 color;

    float metalness;
    float roughness;
    float reflectivity;
    float emissive;

    Texture const *albedomap;
    Texture const *surfacemap;
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

    Asset const *asset;
    std::atomic<State> state;

  protected:
    Material() = default;
};

