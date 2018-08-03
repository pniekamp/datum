//
// Datum - transform component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "scene.h"
#include "storage.h"
#include "datum/math.h"

//|---------------------- TransformComponentStorage -------------------------
//|--------------------------------------------------------------------------

class TransformComponentStorage : public DefaultStorage<lml::Transform, lml::Transform, size_t, size_t, size_t, size_t>
{
  public:
    TransformComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class TransformComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

  protected:

    auto &local(size_t index) const { return data<0>(index); }
    auto &world(size_t index) const { return data<1>(index); }
    auto &parent(size_t index) const { return data<2>(index); }
    auto &firstchild(size_t index) const { return data<3>(index); }
    auto &nextsibling(size_t index) const { return data<4>(index); }
    auto &prevsibling(size_t index) const { return data<5>(index); }

    void set_local(size_t index, lml::Transform const &local) { data<0>(index) = local; }
    void set_world(size_t index, lml::Transform const &world) { data<1>(index) = world; }
    void set_parent(size_t index, size_t parent) { data<2>(index) = parent; }
    void set_firstchild(size_t index, size_t firstchild) { data<3>(index) = firstchild; }
    void set_nextsibling(size_t index, size_t nextsibling) { data<4>(index) = nextsibling; }
    void set_prevsibling(size_t index, size_t prevsibling) { data<5>(index) = prevsibling; }

  protected:

    void add(EntityId entity);

    void remove(EntityId entity) override;

    void reparent(size_t index, size_t parentindex);

    void update(size_t index);

    friend class Scene;
    friend class TransformComponent;
};


//|---------------------- TransformComponent --------------------------------
//|--------------------------------------------------------------------------

class TransformComponent
{
  public:
    friend TransformComponent Scene::add_component<TransformComponent>(Scene::EntityId entity);
    friend TransformComponent Scene::add_component<TransformComponent>(Scene::EntityId entity, lml::Transform local);
    friend TransformComponent Scene::add_component<TransformComponent>(Scene::EntityId entity, TransformComponent parent, lml::Transform local);
    friend TransformComponent Scene::get_component<TransformComponent>(Scene::EntityId entity);

  public:
    TransformComponent() = default;
    TransformComponent(size_t index, TransformComponentStorage *storage);

    lml::Transform const &local() const { return storage->local(index); }
    lml::Transform const &world() const { return storage->world(index); }

    void set_local(lml::Transform const &transform);
    void set_local_defered(lml::Transform const &transform);

    void set_parent(TransformComponent const &parent);

  protected:

    size_t index;
    TransformComponentStorage *storage;
};
