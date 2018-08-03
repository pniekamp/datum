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
    PointLightComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class PointLightComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

  protected:

    auto &entity(size_t index) const { return data<0>(index); }
    auto &range(size_t index) const { return data<1>(index); }
    auto &intensity(size_t index) const { return data<2>(index); }
    auto &attenuation(size_t index) const { return data<3>(index); }

    void set_entity(size_t index, Scene::EntityId entity) { data<0>(index) = entity; }
    void set_range(size_t index, float range) { data<1>(index) = range; }
    void set_intensity(size_t index, lml::Color3 const &intensity) { data<2>(index) = intensity; }
    void set_attenuation(size_t index, lml::Attenuation const &attenuation) { data<3>(index) = attenuation; }

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

    float range() const { return storage->range(index); }
    lml::Color3 const &intensity() const { return storage->intensity(index); }
    lml::Attenuation const &attenuation() const { return storage->attenuation(index); }

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
    SpotLightComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class SpotLightComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

  protected:

    auto &entity(size_t index) const { return data<0>(index); }
    auto &range(size_t index) const { return data<1>(index); }
    auto &cutoff(size_t index) const { return data<2>(index); }
    auto &intensity(size_t index) const { return data<3>(index); }
    auto &attenuation(size_t index) const { return data<4>(index); }
    auto &shadowmap(size_t index) const { return data<5>(index); }

    void set_entity(size_t index, Scene::EntityId entity) { data<0>(index) = entity; }
    void set_range(size_t index, float range) { data<1>(index) = range; }
    void set_cutoff(size_t index, float cutoff) { data<2>(index) = cutoff; }
    void set_intensity(size_t index, lml::Color3 const &intensity) { data<3>(index) = intensity; }
    void set_attenuation(size_t index, lml::Attenuation const &attenuation) { data<4>(index) = attenuation; }
    void set_shadowmap(size_t index, SpotMap const *shadowmap) { data<5>(index) = shadowmap; }

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

    float range() const { return storage->range(index); }
    float cutoff() const { return storage->cutoff(index); }
    lml::Color3 const &intensity() const { return storage->intensity(index); }
    lml::Attenuation const &attenuation() const { return storage->attenuation(index); }
    SpotMap const *shadowmap() const { return storage->shadowmap(index); }

    void set_intensity(lml::Color3 const &intensity, float maxrange);
    void set_attenuation(lml::Attenuation const &attenuation, float maxrange);

  protected:

    size_t index;
    SpotLightComponentStorage *storage;
};

