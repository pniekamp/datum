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

//|---------------------- PointLightComponentStorage ------------------------
//|--------------------------------------------------------------------------

class PointLightComponentStorage : public DefaultStorage<Scene::EntityId, float, lml::Color3, lml::Attenuation>
{
  public:

    enum DataLayout
    {
      entityid = 0,
      lightrange = 1,
      lightintensity = 2,
      lightattenuation = 3,
    };

  public:
    PointLightComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class PointLightComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

  protected:

    void add(EntityId entity, lml::Color3 const &intensity, lml::Attenuation const &attenuation);

    friend class Scene;
    friend class PointLightComponent;
};


//|---------------------- PointLightComponent -------------------------------
//|--------------------------------------------------------------------------

class PointLightComponent
{
  public:
    friend PointLightComponent Scene::add_component<PointLightComponent>(Scene::EntityId entity, lml::Color3 intensity, lml::Attenuation attenuation);
    friend PointLightComponent Scene::get_component<PointLightComponent>(Scene::EntityId entity);

  public:
    PointLightComponent() = default;
    PointLightComponent(size_t index, PointLightComponentStorage *storage);

    float range() const { return storage->data<PointLightComponentStorage::lightrange>(index); }
    lml::Color3 const &intensity() const { return storage->data<PointLightComponentStorage::lightintensity>(index); }
    lml::Attenuation const &attenuation() const { return storage->data<PointLightComponentStorage::lightattenuation>(index); }

    void set_intensity(lml::Color3 const &intensity);
    void set_attenuation(lml::Attenuation const &attenuation);

  protected:

    size_t index;
    PointLightComponentStorage *storage;
};


//|---------------------- SpotLightComponentStorage -------------------------
//|--------------------------------------------------------------------------

class SpotLightComponentStorage : public DefaultStorage<Scene::EntityId, float, float, lml::Color3, lml::Attenuation, SpotMap const *>
{
  public:

    enum DataLayout
    {
      entityid = 0,
      lightrange = 1,
      lightcutoff = 2,
      lightintensity = 3,
      lightattenuation = 4,
      shadowmap = 5
    };

  public:
    SpotLightComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class SpotLightComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

  protected:

    void add(EntityId entity, float cutoff, float maxrange, lml::Color3 const &intensity, lml::Attenuation const &attenuation, SpotMap const *shadowmap);

    friend class Scene;
    friend class SpotLightComponent;
};


//|---------------------- SpotLightComponent --------------------------------
//|--------------------------------------------------------------------------

class SpotLightComponent
{
  public:
    friend SpotLightComponent Scene::add_component<SpotLightComponent>(Scene::EntityId entity, float cutoff, float maxrange, lml::Color3 intensity, lml::Attenuation attenuation, SpotMap const *shadowmap);
    friend SpotLightComponent Scene::get_component<SpotLightComponent>(Scene::EntityId entity);

  public:
    SpotLightComponent() = default;
    SpotLightComponent(size_t index, SpotLightComponentStorage *storage);

    float range() const { return storage->data<SpotLightComponentStorage::lightrange>(index); }
    float cutoff() const { return storage->data<SpotLightComponentStorage::lightcutoff>(index); }
    lml::Color3 const &intensity() const { return storage->data<SpotLightComponentStorage::lightintensity>(index); }
    lml::Attenuation const &attenuation() const { return storage->data<SpotLightComponentStorage::lightattenuation>(index); }
    SpotMap const *shadowmap() const { return storage->data<SpotLightComponentStorage::shadowmap>(index); }

    void set_intensity(lml::Color3 const &intensity, float maxrange);
    void set_attenuation(lml::Attenuation const &attenuation, float maxrange);

  protected:

    size_t index;
    SpotLightComponentStorage *storage;
};

