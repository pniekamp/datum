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

    enum DataLayout
    {
      localtransform = 0,
      worldtransform = 1,
      parentindex = 2,
      firstchildindex = 3,
      nextsiblingindex = 4,
      prevsiblingindex = 5
    };

  public:
    TransformComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class TransformComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

  protected:

    auto &local(size_t index) { return data<localtransform>(index); }
    auto &world(size_t index) { return data<worldtransform>(index); }
    auto &parent(size_t index) { return data<parentindex>(index); }
    auto &firstchild(size_t index) { return data<firstchildindex>(index); }
    auto &nextsibling(size_t index) { return data<nextsiblingindex>(index); }
    auto &prevsibling(size_t index) { return data<prevsiblingindex>(index); }

  protected:

    void add(EntityId entity);

    void remove(EntityId entity) override;

    void set_local(size_t index, lml::Transform const &transform);

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
    TransformComponent(size_t index, TransformComponentStorage *storage);

    lml::Transform const &local() const { return storage->data<TransformComponentStorage::localtransform>(index); }
    lml::Transform const &world() const { return storage->data<TransformComponentStorage::worldtransform>(index); }

    void set_local(lml::Transform const &transform);
    void set_local_defered(lml::Transform const &transform);

    void set_parent(TransformComponent const &parent);

  protected:

    size_t index;
    TransformComponentStorage *storage;
};
