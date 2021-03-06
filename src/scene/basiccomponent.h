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

template<typename T>
class BasicComponentStorage : public DefaultStorage<Scene::EntityId, T>
{
  public:
    BasicComponentStorage(Scene *scene, StackAllocator<> allocator)
      : DefaultStorage<Scene::EntityId, T>(scene, allocator)
    {
    }

  protected:

    using EntityId = Scene::EntityId;

    EntityId const &entityid(size_t index) const
    {
      return std::get<0>(this->m_data)[index];
    }

    T *data(EntityId entity)
    {
      return &std::get<1>(this->m_data)[this->index(entity)];
    }

    T const *data(EntityId entity) const
    {
      return &std::get<1>(this->m_data)[this->index(entity)];
    }

    T *add(EntityId entity)
    {
      auto index = this->insert(entity);

      std::get<0>(this->m_data)[index] = entity;

      return &std::get<1>(this->m_data)[index];
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
  delegate<void(Scene&, Scene::EntityId)> destroyed = [](Scene&, Scene::EntityId) {};
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

  private:

    Example *m_data;
};


class ExampleComponentStorage : public BasicComponentStorage<ExampleComponent, Example>
{
  public:
    using BasicComponentStorage::BasicComponentStorage;

    ExampleComponent get(EntityId entity)
    {
      return data(entity);
    }
};


//|---------------------- ExampleComponent ----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// initialise_component_storage //////////////////////
template<>
void Scene::initialise_component_storage<ExampleComponent>()
{
  m_systems[typeid(ExampleComponentStorage)] = new(allocate<ExampleComponentStorage>(m_allocator)) ExampleComponentStorage(this, m_allocator);
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
ExampleComponent Scene::add_component<ExampleComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<ExampleComponentStorage>()->add(entity);
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
