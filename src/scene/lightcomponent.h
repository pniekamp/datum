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

    void add(EntityId entity, lml::Color3 intensity, lml::Attenuation attenuation);

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


