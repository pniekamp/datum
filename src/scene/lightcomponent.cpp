//
// Datum - light component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "lightcomponent.h"
#include "debug.h"

using namespace std;
using namespace lml;


//|---------------------- LightStorage --------------------------------------
//|--------------------------------------------------------------------------

class LightStoragePrivate : public LightComponentStorage
{
  public:

    typedef StackAllocatorWithFreelist<> allocator_type;

    LightStoragePrivate(Scene *scene, allocator_type const &allocator);

  public:

    auto &range(size_t index) { return std::get<lightrange>(m_data)[index]; }
    auto &intensity(size_t index) { return std::get<lightintensity>(m_data)[index]; }
    auto &attenuation(size_t index) { return std::get<lightattenuation>(m_data)[index]; }

  public:

    void add(EntityId entity, Color3 const &intensity, Attenuation const &attenuation);
    void remove(EntityId entity);

    void update(size_t index);

  private:
};


///////////////////////// LightStorage::Constructor /////////////////////////
LightComponentStorage::LightComponentStorage(Scene *scene, StackAllocatorWithFreelist<> allocator)
  : DefaultStorage(scene, allocator)
{
}


///////////////////////// LightStorage::Constructor /////////////////////////
LightStoragePrivate::LightStoragePrivate(Scene *scene, allocator_type const &allocator)
  : LightComponentStorage(scene, allocator)
{
}


///////////////////////// LightStorage::add /////////////////////////////////
void LightStoragePrivate::add(EntityId entity, Color3 const &intensity, Attenuation const &attenuation)
{
  DefaultStorage::add(entity);

  auto index = this->index(entity);

  get<lightintensity>(m_data)[index] = intensity;
  get<lightattenuation>(m_data)[index] = attenuation;

  update(index);
}


///////////////////////// LightStorage::remove //////////////////////////////
void LightStoragePrivate::remove(EntityId entity)
{
  DefaultStorage::remove(entity);
}


///////////////////////// LightStorage::update //////////////////////////////
void LightStoragePrivate::update(size_t index)
{
  range(index) = lml::range(attenuation(index), max_element(intensity(index)));
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<LightComponent>()
{
  m_systems[typeid(LightComponentStorage)] = new(allocator<LightStoragePrivate>().allocate(1)) LightStoragePrivate(this, allocator());
}


//|---------------------- LightComponent ------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// LightComponent::Constructor ///////////////////////
LightComponent::LightComponent(size_t index, LightComponentStorage *storage)
  : index(index),
    storage(storage)
{
}


//|---------------------- PointLightComponent -------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Scene::add_component //////////////////////////////
template<>
PointLightComponent Scene::add_component<PointLightComponent>(Scene::EntityId entity, lml::Color3 intensity, lml::Attenuation attenuation)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  auto storage = static_cast<LightStoragePrivate*>(system<LightComponentStorage>());

  storage->add(entity, intensity, attenuation);

  return { storage->index(entity), storage };
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<PointLightComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = static_cast<LightStoragePrivate*>(system<LightComponentStorage>());

  storage->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<PointLightComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  return system<LightComponentStorage>()->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
PointLightComponent Scene::get_component<PointLightComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = static_cast<LightStoragePrivate*>(system<LightComponentStorage>());

  return { storage->index(entity), storage };
}
