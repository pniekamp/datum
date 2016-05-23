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

    typedef StackAllocator<> allocator_type;

    LightStoragePrivate(Scene *scene, allocator_type const &allocator);

  public:

    auto &range(size_t index) { return std::get<lightrange>(m_data)[index]; }
    auto &intensity(size_t index) { return std::get<lightintensity>(m_data)[index]; }
    auto &attenuation(size_t index) { return std::get<lightattenuation>(m_data)[index]; }

  public:

    void add(EntityId entity, Color3 const &intensity, Attenuation const &attenuation);

    void set_intensity(size_t index, Color3 const &intensity);
    void set_attenuation(size_t index, Attenuation const &attenuation);

    void update(size_t index);
};


///////////////////////// LightStorage::Constructor /////////////////////////
LightComponentStorage::LightComponentStorage(Scene *scene, StackAllocator<> allocator)
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

  set_intensity(index, intensity);
  set_attenuation(index, attenuation);

  update(index);
}


///////////////////////// LightStorage::set_intensity ///////////////////////
void LightStoragePrivate::set_intensity(size_t index, Color3 const &intensity)
{
  get<lightintensity>(m_data)[index] = intensity;
}


///////////////////////// LightStorage::set_attenuation /////////////////////
void LightStoragePrivate::set_attenuation(size_t index, Attenuation const &attenuation)
{
  get<lightattenuation>(m_data)[index] = attenuation;
}


///////////////////////// LightStorage::update //////////////////////////////
void LightStoragePrivate::update(size_t index)
{
  get<lightrange>(m_data)[index] = lml::range(attenuation(index), max_element(intensity(index)));
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


///////////////////////// LightComponent::set_intensity /////////////////////
void LightComponent::set_intensity(Color3 const &intensity)
{
  static_cast<LightStoragePrivate*>(storage)->set_intensity(index, intensity);

  static_cast<LightStoragePrivate*>(storage)->update(index);
}


///////////////////////// LightComponent::set_attenuation ///////////////////
void LightComponent::set_attenuation(Attenuation const &attenuation)
{
  static_cast<LightStoragePrivate*>(storage)->set_attenuation(index, attenuation);

  static_cast<LightStoragePrivate*>(storage)->update(index);
}


//|---------------------- PointLightComponent -------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Scene::add_component //////////////////////////////
template<>
PointLightComponent Scene::add_component<PointLightComponent>(Scene::EntityId entity, Color3 intensity, Attenuation attenuation)
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
