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
    ParticleSystemComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class ParticleSystemComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

    void update_particlesystem_bounds();

  protected:

    auto &entity(size_t index) const { return data<0>(index); }
    auto &flags(size_t index) const { return data<1>(index); }
    auto &bound(size_t index) const { return data<2>(index); }
    auto &system(size_t index) const { return data<3>(index); }
    auto &instance(size_t index) const { return data<4>(index); }

    void set_entity(size_t index, Scene::EntityId entity) { data<0>(index) = entity; }
    void set_flags(size_t index, int flags) { data<1>(index) = flags; }
    void set_bound(size_t index, lml::Bound3 const &bound) { data<2>(index) = bound; }
    void set_system(size_t index, ParticleSystem const *system) { data<3>(index) = system; }
    void set_instance(size_t index, ParticleSystem::Instance *instance) { data<4>(index) = instance; }

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

    int flags() const { return storage->flags(index); }

    lml::Bound3 const &bound() const { return storage->bound(index); }

    ParticleSystem const *system() const { return storage->system(index); }
    ParticleSystem::Instance *instance() const { return storage->instance(index); }

  protected:

    size_t index;
    ParticleSystemComponentStorage *storage;
};
