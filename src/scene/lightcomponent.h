//
// Datum - light component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "scene.h"
#include "storage.h"
#include "transformcomponent.h"
#include "datum/math.h"
#include "datum/renderer.h"


//|---------------------- LightComponentStorage -----------------------------
//|--------------------------------------------------------------------------

class LightComponentStorage : public DefaultStorage<float, lml::Color3, lml::Attenuation>
{
  public:

    enum DataLayout
    {
      lightrange = 0,
      lightintensity = 1,
      lightattenuation = 2
    };

  public:
    LightComponentStorage(Scene *scene, StackAllocator<> allocator);

    float const &range(EntityId entity) const { return data<lightrange>(index(entity)); }
    lml::Color3 const &intensity(EntityId entity) const { return data<lightintensity>(index(entity)); }
    lml::Attenuation const &attenuation(EntityId entity) const { return data<lightattenuation>(index(entity)); }
};



//|---------------------- LightComponent ------------------------------------
//|--------------------------------------------------------------------------

class LightComponent
{
  public:

    float const &range() const { return storage->data<LightComponentStorage::lightrange>(index); }
    lml::Color3 const &intensity() const { return storage->data<LightComponentStorage::lightintensity>(index); }
    lml::Attenuation const &attenuation() const { return storage->data<LightComponentStorage::lightattenuation>(index); }

    void set_intensity(lml::Color3 const &intensity);
    void set_attenuation(lml::Attenuation const &attenuation);

  protected:
    LightComponent(size_t index, LightComponentStorage *storage);

    size_t index;
    LightComponentStorage *storage;
};


//|---------------------- PointLightComponent -------------------------------
//|--------------------------------------------------------------------------

class PointLightComponent : public LightComponent
{
  public:
    friend PointLightComponent Scene::add_component<PointLightComponent>(Scene::EntityId entity, lml::Color3 intensity, lml::Attenuation attenuation);
    friend PointLightComponent Scene::get_component<PointLightComponent>(Scene::EntityId entity);

  protected:
    using LightComponent::LightComponent;
};
