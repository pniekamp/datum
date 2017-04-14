//
// Datum - scene graph
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "scene.h"
#include "entity.h"
#include "storage.h"
#include "debug.h"

using namespace std;

//|---------------------- Scene ---------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Scene::Constructor ////////////////////////////////
Scene::Scene(allocator_type const &allocator)
  : m_allocator(allocator, m_freelist),
    m_systems(allocator),
    m_slots(allocator),
    m_freeslots(allocator)
{
  m_systems.reserve(128);

  clear();
}


///////////////////////// Scene::clear //////////////////////////////////////
void Scene::clear()
{
  for(auto &system : m_systems)
  {
    system.second->clear();
  }

  for(auto &slot : m_slots)
  {
    if (slot.entity)
    {
      get(slot.id)->~Entity();

      if (slot.bytes != 0)
      {
        m_freelist.release(slot.entity, slot.bytes);
      }
    }
  }

  m_slots.clear();
  m_freeslots.clear();

  m_slots.push_back({ 0, (size_t)-1, nullptr });

  RESOURCE_USE(EntitySlot, m_slots.size(), m_slots.capacity())
}


///////////////////////// Scene::reserve ////////////////////////////////////
void Scene::reserve(size_t capacity)
{
  m_slots.reserve(capacity);

  RESOURCE_USE(EntitySlot, m_slots.size(), m_slots.capacity())
}


///////////////////////// Scene::acquire_slot ///////////////////////////////
Scene::Slot *Scene::acquire_slot()
{
  Slot *slot = nullptr;

  if (m_freeslots.size() != 0)
  {
    slot = &m_slots[m_freeslots.front()];

    m_freeslots.pop_front();
  }
  else
  {
    assert(m_slots.size() < ((1 << kIndexBits) - 1));

    m_slots.push_back({ 0, 0, nullptr });

    slot = &m_slots.back();
  }

  slot->id = { ((slot->id.generation() + 1) << kIndexBits) + (slot - &m_slots.front()) };

  RESOURCE_USE(EntitySlot, m_slots.size(), m_slots.capacity())

  return slot;
}


///////////////////////// Scene::destroy ////////////////////////////////////
void Scene::destroy(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  Slot *slot = &m_slots[entity.index()];

  get(entity)->~Entity();

  if (slot->bytes != 0)
  {
    m_freelist.release(slot->entity, slot->bytes);
  }

  slot->bytes = -1;
  slot->entity = nullptr;

  m_freeslots.push_back(entity.index());

  for(auto &system : m_systems)
  {
    system.second->destroyed(entity);
  }

  RESOURCE_USE(EntitySlot, m_slots.size(), m_slots.capacity())
}

