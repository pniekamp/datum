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

//|---------------------- PointLightComponent -------------------------------
//|--------------------------------------------------------------------------

struct PointLightComponentData
{
  float range;
  lml::Color3 intensity;
  lml::Attenuation attenuation;
};

class PointLightComponent
{
  public:
    friend PointLightComponent Scene::add_component<PointLightComponent>(Scene::EntityId entity, lml::Color3 intensity, lml::Attenuation attenuation);
    friend PointLightComponent Scene::get_component<PointLightComponent>(Scene::EntityId entity);

  public:
    PointLightComponent(PointLightComponentData *data);

    float const &range() const { return m_data->range; }
    lml::Color3 const &intensity() const { return m_data->intensity; }
    lml::Attenuation const &attenuation() const { return m_data->attenuation; }

    void set_intensity(lml::Color3 const &intensity);
    void set_attenuation(lml::Attenuation const &attenuation);

  private:

    PointLightComponentData *m_data;
};

class PointLightComponentStorage : public BasicComponentStorage<PointLightComponentData>
{
  public:
    using BasicComponentStorage::BasicComponentStorage;

    PointLightComponent get(EntityId entity)
    {
      return &data(entity);
    }
};
