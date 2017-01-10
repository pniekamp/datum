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

///////////////////////// PointLightStorage::add ////////////////////////////
PointLightComponentData *PointLightComponentStorage::add(Scene::EntityId entity, Color3 intensity, Attenuation attenuation)
{
  auto data = BasicComponentStorage<PointLightComponentData>::add(entity);

  data->intensity = intensity;
  data->attenuation = attenuation;
  data->range = lml::range(attenuation, max_element(intensity));

  return data;
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<PointLightComponent>()
{
  m_systems[typeid(PointLightComponentStorage)] = new(allocator<PointLightComponentStorage>().allocate(1)) PointLightComponentStorage(this, allocator());
}


//|---------------------- PointLightComponent -------------------------------
//|--------------------------------------------------------------------------

///////////////////////// PointLightComponent::Constructor //////////////////
PointLightComponent::PointLightComponent(PointLightComponentData *data)
  : m_data(data)
{
}


///////////////////////// PointLightComponent::set_intensity ////////////////
void PointLightComponent::set_intensity(Color3 const &intensity)
{
  m_data->intensity = intensity;

  m_data->range = lml::range(m_data->attenuation, max_element(m_data->intensity));
}


///////////////////////// PointLightComponent::set_attenuation //////////////
void PointLightComponent::set_attenuation(Attenuation const &attenuation)
{
  m_data->attenuation = attenuation;

  m_data->range = lml::range(m_data->attenuation, max_element(m_data->intensity));
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
PointLightComponent Scene::add_component<PointLightComponent>(Scene::EntityId entity, Color3 intensity, Attenuation attenuation)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  return system<PointLightComponentStorage>()->add(entity, intensity, attenuation);
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
