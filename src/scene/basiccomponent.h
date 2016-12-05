//
// Basic Component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "scene.h"
#include "storage.h"
#include "delegate.h"

//|---------------------- BasicComponentStorage -----------------------------
//|--------------------------------------------------------------------------

template<typename Data>
class BasicComponentStorage : public DefaultStorage<Scene::EntityId, Data>
{
  public:

    using EntityId = Scene::EntityId;

  public:
    BasicComponentStorage(Scene *scene, StackAllocator<> allocator)
      : DefaultStorage<Scene::EntityId, Data>(scene, allocator)
    {
    }

    class iterator
    {
      public:

        bool operator ==(iterator const &that) const { return index == that.index; }
        bool operator !=(iterator const &that) const { return index != that.index; }

        EntityId const &operator *() const { return storage->template data<0>(index); }
        EntityId const *operator ->() const { return &storage->template data<0>(index); }

        iterator &operator++()
        {
          ++index;

          while (index < storage->size() && storage->template data<0>(index) == 0)
            ++index;

          return *this;
        }

        size_t index;
        BasicComponentStorage *storage;
    };

    template<typename Iterator>
    class iterator_pair : public std::pair<Iterator, Iterator>
    {
      public:
        using std::pair<Iterator, Iterator>::pair;

        Iterator begin() const { return this->first; }
        Iterator end() const { return this->second; }
    };

    iterator_pair<iterator> entities()
    {
      return { ++iterator{ 0, this }, iterator{ this->size(), this } };
    }

  protected:

    Data &data(EntityId entity)
    {
      return std::get<1>(this->m_data)[this->index(entity)];
    }

    Data const &data(EntityId entity) const
    {
      return std::get<1>(this->m_data)[this->index(entity)];
    }

    using DefaultStorage<Scene::EntityId, Data>::data;

    Data &add(Scene::EntityId entity)
    {
      DefaultStorage<Scene::EntityId, Data>::add(entity);

      auto index = this->index(entity);

      std::get<0>(this->m_data)[index] = entity;

      return std::get<1>(this->m_data)[index];
    }

    void remove(Scene::EntityId entity) override
    {
      auto index = this->index(entity);

      std::get<0>(this->m_data)[index] = {};

      DefaultStorage<Scene::EntityId, Data>::remove(entity);
    }

    friend class Scene;
};

/*
 * Example Basic Component
 *

//|---------------------- ExampleComponent ----------------------------------
//|--------------------------------------------------------------------------

struct Example
{
  lml::Vec3 position;
  lml::Vec3 velocity;
  delegate<void(Scene::EntityId)> destroyed;
};

class ExampleComponent
{
  public:
    friend ExampleComponent Scene::add_component<ExampleComponent>(Scene::EntityId entity);
    friend ExampleComponent Scene::get_component<ExampleComponent>(Scene::EntityId entity);

  public:
    ExampleComponent(Example *data)
      : m_data(data)
    {
    }

    ExampleComponent get(EntityId entity)
    {
      return &data(entity);
    }

  private:

    Example *m_data;
};


class ExampleComponentStorage : public BasicComponentStorage<ExampleComponent, Example>
{
  public:
    using BasicComponentStorage::BasicComponentStorage;
};


//|---------------------- ExampleComponent ----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// initialise_component_storage //////////////////////
template<>
void Scene::initialise_component_storage<ExampleComponent>()
{
  m_systems[typeid(ExampleComponentStorage)] = new(allocator<ExampleComponentStorage>().allocate(1)) ExampleComponentStorage(this, allocator());
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
ExampleComponent Scene::add_component<ExampleComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = system<ExampleComponentStorage>();

  auto &example = storage->add(entity);

  return &example;
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<ExampleComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  system<ExampleComponentStorage>()->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<ExampleComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  return system<ExampleComponentStorage>()->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
ExampleComponent Scene::get_component<ExampleComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<ExampleComponentStorage>()->get(entity);
}
*/
