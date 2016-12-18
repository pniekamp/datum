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

///////////////////////// NameComponentStorage::Constructor /////////////////
NameComponentStorage::NameComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator),
    m_names(allocator)
{
  m_names.reserve(16384);
}


///////////////////////// NameComponentStorage::clear ///////////////////////
void NameComponentStorage::clear()
{
  m_names.clear();

  DefaultStorage::clear();
}


///////////////////////// NameComponentStorage::add /////////////////////////
void NameComponentStorage::add(EntityId entity, const char *name)
{
  DefaultStorage::add(entity);

  set_name(entity, name);
}


///////////////////////// NameComponentStorage::set_name ////////////////////
void NameComponentStorage::set_name(EntityId entity, const char *name)
{
  assert(has(entity));

  auto index = this->index(entity);

  data<1>(index) = m_names.size();

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
  m_systems[typeid(NameComponentStorage)] = new(allocator<NameComponentStorage>().allocate(1)) NameComponentStorage(this, allocator());
}



//|---------------------- NameComponent -------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// NameComponent::Constructor ////////////////////////
NameComponent::NameComponent(Scene::EntityId entity, NameComponentStorage *storage)
  : entity(entity),
    storage(storage)
{
}


///////////////////////// NameComponent::set_name ///////////////////////////
void NameComponent::set_name(const char *name)
{
  storage->set_name(entity, name);
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
NameComponent Scene::add_component<NameComponent>(Scene::EntityId entity, const char *name)
{
  assert(get(entity) != nullptr);

  auto storage = system<NameComponentStorage>();

  storage->add(entity, name);

  return { entity, storage };
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<NameComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  system<NameComponentStorage>()->remove(entity);
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

  return system<NameComponentStorage>()->get(entity);
}
