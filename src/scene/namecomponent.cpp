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
    typedef StackAllocator<> allocator_type;

    NameStoragePrivate(Scene *scene, allocator_type allocator);

  public:

    void clear() override;

    void remove(EntityId entity) override;

    void set_name(EntityId entity, const char *name);
};


///////////////////////// NameComponentStorage::Constructor /////////////////
NameComponentStorage::NameComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator),
    m_names(allocator)
{
  m_names.reserve(16384);
}


///////////////////////// NameComponentStorage::Constructor /////////////////
NameStoragePrivate::NameStoragePrivate(Scene *scene, allocator_type allocator)
  : NameComponentStorage(scene, allocator)
{
}


///////////////////////// NameComponentStorage::clear ///////////////////////
void NameStoragePrivate::clear()
{
  m_names.clear();

  DefaultStorage::clear();
}


///////////////////////// NameComponentStorage::remove //////////////////////
void NameStoragePrivate::remove(EntityId entity)
{
  auto index = this->index(entity);

  get<0>(m_data)[index] = {};

  DefaultStorage::remove(entity);
}


///////////////////////// NameComponentStorage::set_name ////////////////////
void NameStoragePrivate::set_name(EntityId entity, const char *name)
{
  assert(has(entity));

  get<1>(m_data)[index(entity)] = m_names.size();

  m_names.insert(m_names.end(), name, name + strlen(name) + 2);
}


///////////////////////// NameComponentStorage::find ////////////////////////
Scene::EntityId NameComponentStorage::find(const char *name) const
{
  for(size_t index = 1; index < size(); ++index)
  {
    if (data<0>(index) != 0)
    {
      if (stricmp(m_names.data() + data<1>(index), name) == 0)
        return data<0>(index);
    }
  }

  return {};
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
