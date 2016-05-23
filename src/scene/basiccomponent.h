//
// Basic Component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "datum/scene.h"
#include "delegate.h"

//|---------------------- BasicComponent ------------------------------------
//|--------------------------------------------------------------------------

template<typename Data>
class BasicComponent
{
  public:

    class Storage : public DefaultStorage<Scene::EntityId, Data>
    {
      public:
        Storage(Scene *scene, StackAllocator<> allocator)
          : DefaultStorage<Scene::EntityId, Data>(scene, allocator)
        {
        }

        Scene::EntityId entity(size_t index) const { return std::get<0>(this->m_data)[index]; }

        Data *data(size_t index) { return &std::get<1>(this->m_data)[index]; }
        Data const *data(size_t index) const { return &std::get<1>(this->m_data)[index]; }

        class iterator
        {
          public:

            Scene::EntityId entity() const { return storage->entity(index); }

            bool operator ==(iterator const &that) const { return index == that.index; }
            bool operator !=(iterator const &that) const { return index != that.index; }

            Data &operator *() { return *storage->data(index); }
            Data *operator ->() { return storage->data(index); }

            iterator &operator++()
            {
              ++index;

              while (index < storage->size() && entity() == 0)
                ++index;

              return *this;
            }

            size_t index;
            Storage *storage;
        };

        template<typename Iterator>
        class iterator_pair : public std::pair<Iterator, Iterator>
        {
          public:
            using std::pair<Iterator, Iterator>::pair;

            Iterator begin() const { return this->first; }
            Iterator end() const { return this->second; }
        };

        iterator_pair<iterator> components()
        {
          return { ++iterator{ 0, this }, iterator{ this->size(), this } };
        }

      protected:

        Data *add(Scene::EntityId entity)
        {
          DefaultStorage<Scene::EntityId, Data>::add(entity);

          auto index = this->index(entity);

          std::get<0>(this->m_data)[index] = entity;

          return data(index);
        }

        void remove(Scene::EntityId entity) override
        {
          auto index = this->index(entity);

          std::get<0>(this->m_data)[index] = {};

          DefaultStorage<Scene::EntityId, Data>::remove(entity);
        }

        friend class Scene;
    };

  public:

    Data *operator ->() { return storage->data(index); }

  protected:
    BasicComponent(size_t index, Storage *storage)
      : index(index), storage(storage)
    {
    }

    size_t index;
    Storage *storage;

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

using ExampleComponent = BasicComponent<Example>;

template<>
ExampleComponent Scene::add_component<ExampleComponent>(Scene::EntityId entity);


//|---------------------- ExampleComponent ----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// initialise_component_storage //////////////////////
template<>
void Scene::initialise_component_storage<ExampleComponent>()
{
  m_systems[typeid(ExampleComponent::Storage)] = new(allocator<ExampleComponent::Storage>().allocate(1)) ExampleComponent::Storage(this, allocator());
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
ExampleComponent Scene::add_component<ExampleComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = system<ExampleComponent::Storage>();

  auto example = storage->add(entity);

  return { storage->index(entity), storage };
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<ExampleComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = system<ExampleComponent::Storage>();

  storage->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<ExampleComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  auto storage = system<ExampleComponent::Storage>();

  return storage->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
ExampleComponent Scene::get_component<ExampleComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = system<ExampleComponent::Storage>();

  return { storage->index(entity), storage };
}

*/
