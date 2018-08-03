//
// Datum - transform component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "transformcomponent.h"
#include "debug.h"

using namespace std;
using namespace lml;

//|---------------------- TransformStorage ----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// TransformStorage::Constructor /////////////////////
TransformComponentStorage::TransformComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator)
{
}


///////////////////////// TransformStorage::add /////////////////////////////
void TransformComponentStorage::add(EntityId entity)
{
  auto index = insert(entity);

  set_parent(index, 0);
  set_firstchild(index, 0);
  set_nextsibling(index, 0);
  set_prevsibling(index, 0);
}


///////////////////////// TransformStorage::remove //////////////////////////
void TransformComponentStorage::remove(EntityId entity)
{
  assert(has(entity));
  assert(firstchild(index(entity)) == 0);

  auto index = this->index(entity);

  if (firstchild(parent(index)) == index)
    set_firstchild(parent(index), nextsibling(index));

  set_nextsibling(prevsibling(index), nextsibling(index));
  set_prevsibling(nextsibling(index), prevsibling(index));

  DefaultStorage::remove(entity);
}


///////////////////////// TransformStorage::reparent ////////////////////////
void TransformComponentStorage::reparent(size_t index, size_t parentindex)
{
  if (firstchild(parent(index)) == index)
    set_firstchild(parent(index), nextsibling(index));

  set_nextsibling(prevsibling(index), nextsibling(index));
  set_prevsibling(nextsibling(index), prevsibling(index));

  set_parent(index, parentindex);
  set_nextsibling(index, firstchild(parentindex));
  set_prevsibling(index, 0);
  set_firstchild(parentindex, index);
  set_prevsibling(nextsibling(index), index);
}


///////////////////////// TransformStorage::update //////////////////////////
void TransformComponentStorage::update(size_t index)
{
  set_world(index, parent(index) ? world(parent(index)) * local(index) : local(index));

  for(size_t child = firstchild(index); child != 0; child = nextsibling(child))
  {
    update(child);
  }
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<TransformComponent>()
{
  m_systems[typeid(TransformComponentStorage)] = new(allocate<TransformComponentStorage>(m_allocator)) TransformComponentStorage(this, m_allocator);
}


//|---------------------- TransformComponent --------------------------------
//|--------------------------------------------------------------------------

///////////////////////// TransformComponent::Constructor ///////////////////
TransformComponent::TransformComponent(size_t index, TransformComponentStorage *storage)
  : index(index),
    storage(storage)
{
}


///////////////////////// TransformComponent::set_local /////////////////////
void TransformComponent::set_local(Transform const &transform)
{
  storage->set_local(index, transform);

  storage->update(index);
}


///////////////////////// TransformComponent::set_local /////////////////////
void TransformComponent::set_local_defered(Transform const &transform)
{
  storage->set_local(index, transform);
}


///////////////////////// TransformComponent::set_parent ////////////////////
void TransformComponent::set_parent(TransformComponent const &parent)
{
  assert(parent.storage == storage);

  storage->reparent(index, parent.index);

  storage->update(index);
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
TransformComponent Scene::add_component<TransformComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = system<TransformComponentStorage>();

  storage->add(entity);

  return { storage->index(entity), storage };
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
TransformComponent Scene::add_component<TransformComponent>(Scene::EntityId entity, Transform local)
{
  auto transform = add_component<TransformComponent>(entity);

  transform.set_local(local);

  return transform;
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
TransformComponent Scene::add_component<TransformComponent>(Scene::EntityId entity, TransformComponent parent, Transform local)
{
  auto transform = add_component<TransformComponent>(entity);

  transform.set_local_defered(local);
  transform.set_parent(parent);

  return transform;
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<TransformComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  system<TransformComponentStorage>()->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<TransformComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  return system<TransformComponentStorage>()->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
TransformComponent Scene::get_component<TransformComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<TransformComponentStorage>()->get(entity);
}
