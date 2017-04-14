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
void PointLightComponentStorage::add(Scene::EntityId entity, Color3 intensity, Attenuation attenuation)
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
  m_systems[typeid(PointLightComponentStorage)] = new(allocator<PointLightComponentStorage>().allocate(1)) PointLightComponentStorage(this, m_allocator);
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
