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

//|---------------------- PointLightStorage ---------------------------------
//|--------------------------------------------------------------------------

///////////////////////// PointLightStorage::Constructor //////////////////////////
PointLightComponentStorage::PointLightComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator)
{
}


///////////////////////// PointLightStorage::add ////////////////////////////
void PointLightComponentStorage::add(Scene::EntityId entity, Color3 const &intensity, Attenuation const &attenuation)
{
  auto index = DefaultStorage::add(entity);

  data<entityid>(index) = entity;
  data<lightintensity>(index) = intensity;
  data<lightattenuation>(index) = attenuation;
  data<lightrange>(index) = range(attenuation, max_element(intensity));
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<PointLightComponent>()
{
  m_systems[typeid(PointLightComponentStorage)] = new(allocate<PointLightComponentStorage>(m_allocator)) PointLightComponentStorage(this, m_allocator);
}


//|---------------------- PointLightComponent -------------------------------
//|--------------------------------------------------------------------------

///////////////////////// PointLightComponent::Constructor //////////////////
PointLightComponent::PointLightComponent(size_t index, PointLightComponentStorage *storage)
  : index(index),
    storage(storage)
{
}


///////////////////////// PointLightComponent::set_intensity ////////////////
void PointLightComponent::set_intensity(Color3 const &intensity)
{
  storage->data<PointLightComponentStorage::lightintensity>(index) = intensity;

  storage->data<PointLightComponentStorage::lightrange>(index) = lml::range(attenuation(), max_element(intensity()));
}


///////////////////////// PointLightComponent::set_attenuation //////////////
void PointLightComponent::set_attenuation(Attenuation const &attenuation)
{
  storage->data<PointLightComponentStorage::lightattenuation>(index) = attenuation;

  storage->data<PointLightComponentStorage::lightrange>(index) = lml::range(attenuation(), max_element(intensity()));
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
PointLightComponent Scene::add_component<PointLightComponent>(Scene::EntityId entity, Color3 intensity, Attenuation attenuation)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  auto storage = system<PointLightComponentStorage>();

  storage->add(entity, intensity, attenuation);

  return { storage->index(entity), storage };
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<PointLightComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  system<PointLightComponentStorage>()->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<PointLightComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  return system<PointLightComponentStorage>()->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
PointLightComponent Scene::get_component<PointLightComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<PointLightComponentStorage>()->get(entity);
}


//|---------------------- SpotLightStorage ----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// SpotLightStorage::Constructor //////////////////////////
SpotLightComponentStorage::SpotLightComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator)
{
}


///////////////////////// SpotLightStorage::add ////////////////////////////
void SpotLightComponentStorage::add(Scene::EntityId entity, float cutoff, float maxrange, Color3 const &intensity, Attenuation const &attenuation, SpotMap const *spotmap)
{
  auto index = DefaultStorage::add(entity);

  data<entityid>(index) = entity;
  data<lightintensity>(index) = intensity;
  data<lightattenuation>(index) = attenuation;
  data<lightrange>(index) = min(range(attenuation, max_element(intensity)), maxrange);
  data<lightcutoff>(index) = cutoff;
  data<shadowmap>(index) = spotmap;
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<SpotLightComponent>()
{
  m_systems[typeid(SpotLightComponentStorage)] = new(allocate<SpotLightComponentStorage>(m_allocator)) SpotLightComponentStorage(this, m_allocator);
}


//|---------------------- SpotLightComponent --------------------------------
//|--------------------------------------------------------------------------

///////////////////////// SpotLightComponent::Constructor //////////////////
SpotLightComponent::SpotLightComponent(size_t index, SpotLightComponentStorage *storage)
  : index(index),
    storage(storage)
{
}


///////////////////////// SpotLightComponent::set_intensity ////////////////
void SpotLightComponent::set_intensity(Color3 const &intensity, float maxrange)
{
  storage->data<SpotLightComponentStorage::lightintensity>(index) = intensity;

  storage->data<SpotLightComponentStorage::lightrange>(index) = min(lml::range(attenuation(), max_element(intensity())), maxrange);
}


///////////////////////// SpotLightComponent::set_attenuation //////////////
void SpotLightComponent::set_attenuation(Attenuation const &attenuation, float maxrange)
{
  storage->data<SpotLightComponentStorage::lightattenuation>(index) = attenuation;

  storage->data<SpotLightComponentStorage::lightrange>(index) = min(lml::range(attenuation(), max_element(intensity())), maxrange);
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
SpotLightComponent Scene::add_component<SpotLightComponent>(Scene::EntityId entity, float cutoff, float maxrange, Color3 intensity, Attenuation attenuation, SpotMap const *shadowmap)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  auto storage = system<SpotLightComponentStorage>();

  storage->add(entity, cutoff, maxrange, intensity, attenuation, shadowmap);

  return { storage->index(entity), storage };
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<SpotLightComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  system<SpotLightComponentStorage>()->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<SpotLightComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  return system<SpotLightComponentStorage>()->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
SpotLightComponent Scene::get_component<SpotLightComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<SpotLightComponentStorage>()->get(entity);
}
