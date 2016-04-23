//
// Datum - name component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "namecomponent.h"
#include <leap.h>
#include "debug.h"

using namespace std;
using namespace leap;


//|---------------------- NameComponentStorage ------------------------------
//|--------------------------------------------------------------------------

class NameStoragePrivate : public NameComponentStorage
{
  public:
    typedef StackAllocatorWithFreelist<> allocator_type;

    NameStoragePrivate(Scene *scene, allocator_type allocator);

  public:

    auto &names() { return std::get<0>(m_data); }

  public:

    using DefaultStorage::add;
    using DefaultStorage::remove;

    void set_name(EntityId entity, const char *name);

  private:
};


///////////////////////// NameComponentStorage::Constructor /////////////////
NameComponentStorage::NameComponentStorage(Scene *scene, StackAllocatorWithFreelist<> allocator)
  : DefaultStorage(scene, allocator)
{
}


///////////////////////// NameComponentStorage::Constructor /////////////////
NameStoragePrivate::NameStoragePrivate(Scene *scene, allocator_type allocator)
  : NameComponentStorage(scene, allocator)
{
  names().reserve(4096);
}


///////////////////////// NameComponentStorage::set_name ////////////////////
void NameStoragePrivate::set_name(EntityId entity, const char *name)
{
  assert(has(entity));

  m_index[entity.index()] = names().size();

  names().insert(names().end(), name, name + strlen(name) + 2);
}


///////////////////////// NameComponentStorage::find ////////////////////////
Scene::EntityId NameComponentStorage::find(const char *name) const
{
  for(auto &index : m_index)
  {
    if (index != 0)
    {
      if (stricmp(&data<0>(index), name) == 0)
        return entityid(&index - &m_index.front());
    }
  }

  return Scene::EntityId{};
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<NameComponent>()
{
  m_systems[typeid(NameComponentStorage)] = new(allocator<NameStoragePrivate>().allocate(1)) NameStoragePrivate(this, allocator());
}



//|---------------------- NameComponent -------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Scene::add_component //////////////////////////////
template<>
NameComponent Scene::add_component<NameComponent>(Scene::EntityId entity, const char *name)
{
  assert(get(entity) != nullptr);

  auto storage = static_cast<NameStoragePrivate*>(system<NameComponentStorage>());

  storage->add(entity);

  storage->set_name(entity, name);

  return { entity, storage };
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<NameComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = static_cast<NameStoragePrivate*>(system<NameComponentStorage>());

  storage->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<NameComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  return system<NameComponentStorage>()->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
NameComponent Scene::get_component<NameComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = static_cast<NameStoragePrivate*>(system<NameComponentStorage>());

  return { entity, storage };
}


///////////////////////// NameComponent::Constructor ////////////////////////
NameComponent::NameComponent(Scene::EntityId entity, NameComponentStorage *storage)
  : entity(entity),
    storage(storage)
{
}


///////////////////////// NameComponent::set_name ///////////////////////////
void NameComponent::set_name(const char *name)
{
  static_cast<NameStoragePrivate*>(storage)->set_name(entity, name);
}
