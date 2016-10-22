//
// Datum - scene graph
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "datum.h"
#include "datum/memory.h"
#include <iostream>
#include <vector>
#include <deque>
#include <unordered_map>
#include <typeindex>
#include <utility>

class Entity;
class Storage;

//|---------------------- Scene ---------------------------------------------
//|--------------------------------------------------------------------------

class Scene
{
  public:

    typedef StackAllocator<> allocator_type;

    Scene(allocator_type const &allocator);

    Scene(Scene const &) = delete;

  public:

    static const int kIndexBits = 24;

    struct EntityId
    {
      size_t id = 0;

      operator bool() const { return id != 0; }

      size_t index() const { return id & ((1 << kIndexBits) - 1); }
      size_t generation() const { return id >> kIndexBits; }

      bool operator ==(EntityId const &other) const { return id == other.id; }
      bool operator !=(EntityId const &other) const { return id != other.id; }
    };

    // create entity
    template<typename Entity, typename ...Args>
    EntityId create(Args... args);

    template<typename Entity, typename ...Args>
    EntityId load(DatumPlatform::PlatformInterface &platform, Args... args);

    // destroy entity
    void destroy(EntityId entity);

    // access entity
    template<typename Entity = Entity>
    Entity const *get(EntityId entity) const
    {
      auto slot = &m_slots[entity.index()];

      if (slot->id != entity)
        return nullptr;

      return (slot->bytes == 0) ? reinterpret_cast<Entity const *>(&slot->entity) : static_cast<Entity const *>(slot->entity);
    }

    template<typename Entity = Entity>
    Entity *get(EntityId entity)
    {
      return const_cast<Entity*>(static_cast<Scene const &>(*this).get<Entity>(entity));
    }

  public:

    void clear();

    void reserve(size_t capacity);

    template<typename Component>
    void initialise_component_storage();

  public:

    template<typename Component, typename ...Args>
    Component add_component(EntityId entity, Args... args);

    template<typename Component>
    void remove_component(EntityId entity);

    template<typename Component>
    bool has_component(EntityId entity) const;

    template<typename Component>
    Component get_component(EntityId entity);

    template<typename Component, typename ...Rest>
    bool has_components(EntityId entity) const
    {
      return has_component<Component>(entity) && has_components<Rest...>(entity);
    }

    template<typename ...Rest, typename std::enable_if<sizeof...(Rest) == 0>::type* = nullptr>
    bool has_components(EntityId entity) const
    {
      return true;
    }

  public:

    template<typename ...Components>
    class iterator
    {
      public:
        explicit iterator(Scene const *scene, size_t index)
          : m_scene(scene), m_index(index)
        {
          if (m_index != m_scene->m_slots.size() && (!m_scene->get(*this) || !m_scene->has_components<Components...>(*this)))
            ++*this;
        }

        bool operator ==(iterator const &that) const { return m_index == that.m_index; }
        bool operator !=(iterator const &that) const { return m_index != that.m_index; }

        operator EntityId() { return m_scene->m_slots[m_index].id; }

        EntityId const operator *() { return m_scene->m_slots[m_index].id; }

        iterator &operator++()
        {
          ++m_index;

          while (m_index != m_scene->m_slots.size() && (!m_scene->get(*this) || !m_scene->has_components<Components...>(*this)))
            ++m_index;

          return *this;
        }

      private:

        Scene const *m_scene;
        size_t m_index;
    };

    template<typename Iterator>
    class iterator_pair : public std::pair<Iterator, Iterator>
    {
      public:
        using std::pair<Iterator, Iterator>::pair;

        Iterator begin() const { return this->first; }
        Iterator end() const { return this->second; }
    };

    template<typename ...Components>
    auto entities() const { return iterator_pair<iterator<Components...>>{ iterator<Components...>(this, 0), iterator<Components...>(this, m_slots.size()) }; }

  public:

    template<typename System>
    System const *system() const
    {
      assert(m_systems.find(typeid(System)) != m_systems.end());

      return static_cast<System*>(m_systems.find(typeid(System))->second);
    }

    template<typename System>
    System *system()
    {
      return const_cast<System*>(static_cast<const Scene&>(*this).system<System>());
    }

  private:

    FreeList m_freelist;
    allocator_type m_allocator;

    template<typename T = char>
    StackAllocatorWithFreelist<T> allocator()
    {
      return StackAllocatorWithFreelist<T>(m_allocator, m_freelist);
    }

    template<typename Entity, typename ...Args, std::enable_if_t<sizeof(Entity) == sizeof(Entity*)>* = nullptr>
    EntityId add_entity(Args ...args)
    {
      Slot *slot = acquire_slot();

      slot->bytes = 0;
      new(&slot->entity) Entity(std::forward<Args>(args)...);

      return slot->id;
    }

    template<typename Entity, typename ...Args, std::enable_if_t<sizeof(Entity) != sizeof(Entity*)>* = nullptr>
    EntityId add_entity(Args ...args)
    {
      Slot *slot = acquire_slot();

      slot->bytes = sizeof(Entity);
      slot->entity = new(allocator<Entity>().allocate(1)) Entity(std::forward<Args>(args)...);

      return slot->id;
    }

  private:

    friend class Storage;

    std::unordered_map<std::type_index, Storage*, std::hash<std::type_index>, std::equal_to<>, StackAllocator<>> m_systems;

  private:

    struct Slot
    {
      EntityId id;

      size_t bytes;

      Entity *entity;      
    };

    Slot *acquire_slot();

    std::vector<Slot, StackAllocator<>> m_slots;

    std::deque<size_t, StackAllocator<>> m_freeslots;
};


//////////////////////// Entity stream << ///////////////////////////////////
inline std::ostream &operator <<(std::ostream &os, Scene::EntityId const &entity)
{
  os << "[Entity:" << std::hex << entity.id << std::dec << "]";

  return os;
}
