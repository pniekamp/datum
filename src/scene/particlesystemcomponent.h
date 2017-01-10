//
// Datum - particle system component
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

//|---------------------- ParticleSystemComponentStorage --------------------
//|--------------------------------------------------------------------------

struct ParticleSystemComponentData
{
  long flags;
  lml::Bound3 bound;
  ParticleSystem *system;
  ParticleSystem::Instance const *instance;
};

class ParticleSystemComponentStorage : public BasicComponentStorage<ParticleSystemComponentData>
{
  public:
    using BasicComponentStorage::BasicComponentStorage;

    template<typename Component = class ParticleSystemComponent>
    Component get(EntityId entity)
    {
      return data(entity);
    }

  protected:

    ParticleSystemComponentData *add(EntityId entity, lml::Bound3 const &bound, ParticleSystem *particlesystem, long flags);

    void remove(EntityId entity) override;

    void update_particlesystem_bounds();

    friend class Scene;
    friend class ParticleSystemComponent;
    friend void update_particlesystem_bounds(Scene &scene);
};

///////////////////////// update_particlesystem_bounds //////////////////////
void update_particlesystem_bounds(Scene &scene);


//|---------------------- ParticleSystemComponent ---------------------------
//|--------------------------------------------------------------------------

class ParticleSystemComponent
{
  public:

    enum Flags
    {
      Visible = 0x01,
    };

  public:
    friend ParticleSystemComponent Scene::add_component<ParticleSystemComponent>(Scene::EntityId entity, ParticleSystem *particlesystem, long flags);
    friend ParticleSystemComponent Scene::get_component<ParticleSystemComponent>(Scene::EntityId entity);

  public:
    ParticleSystemComponent(ParticleSystemComponentData *data);

    long flags() const { return m_data->flags; }

    lml::Bound3 const &bound() const { return m_data->bound; }

    ParticleSystem const *system() const { return m_data->system; }
    ParticleSystem::Instance const *instance() const { return m_data->instance; }

  private:

    ParticleSystemComponentData *m_data;
};
