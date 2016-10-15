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

    lml::Transform const &local(EntityId entity) const { return data<localtransform>(index(entity)); }
    lml::Transform const &world(EntityId entity) const { return data<worldtransform>(index(entity)); }
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

    lml::Transform const &local() const { return storage->data<TransformComponentStorage::localtransform>(index); }
    lml::Transform const &world() const { return storage->data<TransformComponentStorage::worldtransform>(index); }

    void set_local(lml::Transform const &transform);
    void set_local_defered(lml::Transform const &transform);

    void set_parent(TransformComponent const &parent);

  public:

    template<typename TransformComponent>
    class iterator
    {
      public:
        explicit iterator(size_t index, TransformComponentStorage *storage) : node(index, storage) { }

        bool operator ==(iterator const &that) const { return node.index == that.node.index; }
        bool operator !=(iterator const &that) const { return node.index != that.node.index; }

        TransformComponent &operator *() { return node; }
        TransformComponent *operator ->() { return &node; }

        iterator &operator++()
        {
          node.index = node.storage->template data<TransformComponentStorage::nextsiblingindex>(node.index);

          return *this;
        }

      private:

        TransformComponent node;
    };

    template<typename Iterator>
    class iterator_pair : public std::pair<Iterator, Iterator>
    {
      public:
        using std::pair<Iterator, Iterator>::pair;

        Iterator begin() const { return this->first; }
        Iterator end() const { return this->second; }
    };

    auto children() { return iterator_pair<iterator<TransformComponent>>{ iterator<TransformComponent>(storage->data<TransformComponentStorage::firstchildindex>(index), storage), iterator<TransformComponent>(0, storage) }; }

  private:
    TransformComponent(size_t index, TransformComponentStorage *storage);

    size_t index;
    TransformComponentStorage *storage;
};
