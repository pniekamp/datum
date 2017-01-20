//
// Datum - light component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "scene.h"
#include "basiccomponent.h"
#include "transformcomponent.h"
#include "datum/math.h"
#include "datum/renderer.h"

//|---------------------- PointLightComponentStorage ------------------------
//|--------------------------------------------------------------------------

struct PointLightComponentData
{
  float range;
  lml::Color3 intensity;
  lml::Attenuation attenuation;
};

class PointLightComponentStorage : public BasicComponentStorage<PointLightComponentData>
{
  public:
    using BasicComponentStorage::BasicComponentStorage;

    template<typename Component = class PointLightComponent>
    Component get(EntityId entity)
    {
      return data(entity);
    }

  protected:

    PointLightComponentData *add(EntityId entity, lml::Color3 intensity, lml::Attenuation attenuation);

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
    PointLightComponent(PointLightComponentData *data);

    float range() const { return m_data->range; }
    lml::Color3 const &intensity() const { return m_data->intensity; }
    lml::Attenuation const &attenuation() const { return m_data->attenuation; }

    void set_intensity(lml::Color3 const &intensity);
    void set_attenuation(lml::Attenuation const &attenuation);

  private:

    PointLightComponentData *m_data;
};


