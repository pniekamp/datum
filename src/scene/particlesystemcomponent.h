//
// Datum - particle system component
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

//|---------------------- ParticleSystemComponentStorage --------------------
//|--------------------------------------------------------------------------

class ParticleSystemComponentStorage : public DefaultStorage<Scene::EntityId, int, lml::Bound3, ParticleSystem const *, ParticleSystem::Instance *>
{
  public:

    enum DataLayout
    {
      entityid = 0,
      flagbits = 1,
      boundingbox = 2,
      particlesystem = 3,
      particlesysteminstance = 4,
    };

  public:
    ParticleSystemComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class ParticleSystemComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

    void update_particlesystem_bounds();

  protected:

    void add(EntityId entity, lml::Bound3 const &bound, ParticleSystem const *system, int flags);

    void remove(EntityId entity) override;

  protected:

    FreeList m_freelist;
    StackAllocatorWithFreelist<> m_allocator;

    friend class Scene;
    friend class ParticleSystemComponent;
};

///////////////////////// update_particlesystems ////////////////////////////
void update_particlesystems(Scene &scene, Camera const &camera, float dt);


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
    friend ParticleSystemComponent Scene::add_component<ParticleSystemComponent>(Scene::EntityId entity, ParticleSystem const *particlesystem, int flags);
    friend ParticleSystemComponent Scene::get_component<ParticleSystemComponent>(Scene::EntityId entity);

  public:
    ParticleSystemComponent() = default;
    ParticleSystemComponent(size_t index, ParticleSystemComponentStorage *storage);

    int flags() const { return storage->data<ParticleSystemComponentStorage::flagbits>(index); }

    lml::Bound3 const &bound() const { return storage->data<ParticleSystemComponentStorage::boundingbox>(index); }

    ParticleSystem const *system() const { return storage->data<ParticleSystemComponentStorage::particlesystem>(index); }
    ParticleSystem::Instance *instance() const { return storage->data<ParticleSystemComponentStorage::particlesysteminstance>(index); }

  protected:

    size_t index;
    ParticleSystemComponentStorage *storage;
};
