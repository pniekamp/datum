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

class TransformStoragePrivate : public TransformComponentStorage
{
  public:

    typedef StackAllocator<> allocator_type;

    TransformStoragePrivate(Scene *scene, allocator_type const &allocator);

  public:

    auto &local(size_t index) { return std::get<localtransform>(m_data)[index]; }
    auto &world(size_t index) { return std::get<worldtransform>(m_data)[index]; }
    auto &parent(size_t index) { return std::get<parentindex>(m_data)[index]; }
    auto &firstchild(size_t index) { return std::get<firstchildindex>(m_data)[index]; }
    auto &nextsibling(size_t index) { return std::get<nextsiblingindex>(m_data)[index]; }
    auto &prevsibling(size_t index) { return std::get<prevsiblingindex>(m_data)[index]; }

  public:

    void add(EntityId entity);

    void remove(EntityId entity) override;

    void set_local(size_t index, Transform const &transform);

    void reparent(size_t index, size_t parentindex);

    void update(size_t index);
};


///////////////////////// TransformStorage::Constructor /////////////////////
TransformComponentStorage::TransformComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator)
{
}


///////////////////////// TransformStorage::Constructor /////////////////////
TransformStoragePrivate::TransformStoragePrivate(Scene *scene, allocator_type const &allocator)
  : TransformComponentStorage(scene, allocator)
{
}


///////////////////////// TransformStorage::add /////////////////////////////
void TransformStoragePrivate::add(EntityId entity)
{
  DefaultStorage::add(entity);

  auto index = this->index(entity);

  parent(index) = 0;
  firstchild(index) = 0;
  nextsibling(index) = 0;
  prevsibling(index) = 0;
}


///////////////////////// TransformStorage::remove //////////////////////////
void TransformStoragePrivate::remove(EntityId entity)
{
  assert(has(entity));
  assert(firstchild(index(entity)) == 0);

  auto index = this->index(entity);

  if (firstchild(parent(index)) == index)
    firstchild(parent(index)) = nextsibling(index);

  nextsibling(prevsibling(index)) = nextsibling(index);
  prevsibling(nextsibling(index)) = prevsibling(index);

  DefaultStorage::remove(entity);
}


///////////////////////// TransformStorage::set_local ///////////////////////
void TransformStoragePrivate::set_local(size_t index, Transform const &transform)
{
  local(index) = transform;
}


///////////////////////// TransformStorage::reparent ////////////////////////
void TransformStoragePrivate::reparent(size_t index, size_t parentindex)
{
  if (firstchild(parent(index)) == index)
    firstchild(parent(index)) = nextsibling(index);

  nextsibling(prevsibling(index)) = nextsibling(index);
  prevsibling(nextsibling(index)) = prevsibling(index);

  parent(index) = parentindex;
  nextsibling(index) = firstchild(parentindex);
  prevsibling(index) = 0;
  firstchild(parentindex) = index;
  prevsibling(nextsibling(index)) = index;
}


///////////////////////// TransformStorage::update //////////////////////////
void TransformStoragePrivate::update(size_t index)
{
  world(index) = parent(index) ? world(parent(index)) * local(index) : local(index);

  for(size_t child = firstchild(index); child != 0; child = nextsibling(child))
  {
    update(child);
  }
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<TransformComponent>()
{
  m_systems[typeid(TransformComponentStorage)] = new(allocator<TransformStoragePrivate>().allocate(1)) TransformStoragePrivate(this, allocator());
}



//|---------------------- TransformComponent --------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Scene::add_component //////////////////////////////
template<>
TransformComponent Scene::add_component<TransformComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = static_cast<TransformStoragePrivate*>(system<TransformComponentStorage>());

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

  auto storage = static_cast<TransformStoragePrivate*>(system<TransformComponentStorage>());

  storage->remove(entity);
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

  auto storage = static_cast<TransformStoragePrivate*>(system<TransformComponentStorage>());

  return { storage->index(entity), storage };
}


///////////////////////// TransformComponent::Constructor ///////////////////
TransformComponent::TransformComponent(size_t index, TransformComponentStorage *storage)
  : index(index),
    storage(storage)
{
}


///////////////////////// TransformComponent::set_local /////////////////////
void TransformComponent::set_local(Transform const &transform)
{
  static_cast<TransformStoragePrivate*>(storage)->set_local(index, transform);

  static_cast<TransformStoragePrivate*>(storage)->update(index);
}


///////////////////////// TransformComponent::set_local /////////////////////
void TransformComponent::set_local_defered(Transform const &transform)
{
  static_cast<TransformStoragePrivate*>(storage)->set_local(index, transform);
}


///////////////////////// TransformComponent::set_parent ////////////////////
void TransformComponent::set_parent(TransformComponent const &parent)
{
  assert(parent.storage == storage);

  static_cast<TransformStoragePrivate*>(storage)->reparent(index, parent.index);

  static_cast<TransformStoragePrivate*>(storage)->update(index);
}

