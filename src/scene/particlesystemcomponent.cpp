//
// Datum - particle system component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "particlesystemcomponent.h"
#include "debug.h"

using namespace std;
using namespace lml;

//|---------------------- ParticleSystemStorage -----------------------------
//|--------------------------------------------------------------------------

///////////////////////// ParticleSystemStorage::add ////////////////////////
ParticleSystemComponentData *ParticleSystemComponentStorage::add(EntityId entity, Bound3 const &bound, ParticleSystem *particlesystem, long flags)
{
  auto data = BasicComponentStorage<ParticleSystemComponentData>::add(entity);

  data->flags = flags;
  data->bound = bound;
  data->system = particlesystem;
  data->instance = particlesystem->create();

  return data;
}


///////////////////////// ParticleSystemStorage::remove /////////////////////
void ParticleSystemComponentStorage::remove(EntityId entity)
{
  auto data = this->data(entity);

  data->system->destroy(data->instance);

  DefaultStorage::remove(entity);
}


///////////////////////// update_particlesystem_bounds //////////////////////
void ParticleSystemComponentStorage::update_particlesystem_bounds()
{
  auto transformstorage = m_scene->system<TransformComponentStorage>();

  for(auto &entity : entities())
  {
    assert(transformstorage->has(entity));

    auto data = this->data(entity);
    auto transform = transformstorage->get(entity);

    data->bound = transform.world() * data->system->bound;
  }
}


///////////////////////// update_particlesystems ////////////////////////////
void update_particlesystems(Scene &scene, Camera const &camera, float dt)
{
  auto frustum = camera.frustum();

  auto particlestorage = scene.system<ParticleSystemComponentStorage>();
  auto transformstorage = scene.system<TransformComponentStorage>();

  for(auto &entity : particlestorage->entities())
  {
    auto particles = particlestorage->get(entity);

    if (intersects(frustum, particles.bound()))
    {
      auto transform = transformstorage->get(entity);

      particles.system()->update(particles.instance(), camera, transform.world(), dt);
    }
  }
}


///////////////////////// update_particlesystem_bounds //////////////////////
void update_particlesystem_bounds(Scene &scene)
{
  scene.system<ParticleSystemComponentStorage>()->update_particlesystem_bounds();
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<ParticleSystemComponent>()
{
  m_systems[typeid(ParticleSystemComponentStorage)] = new(allocator<ParticleSystemComponentStorage>().allocate(1)) ParticleSystemComponentStorage(this, allocator());
}



//|---------------------- ParticleSystemComponent ---------------------------
//|--------------------------------------------------------------------------

///////////////////////// ParticleSystemComponent::Constructor //////////////
ParticleSystemComponent::ParticleSystemComponent(ParticleSystemComponentData *data)
  : m_data(data)
{
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
ParticleSystemComponent Scene::add_component<ParticleSystemComponent>(Scene::EntityId entity, ParticleSystem *particlesystem, long flags)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  auto storage = system<ParticleSystemComponentStorage>();

  auto transform = system<TransformComponentStorage>()->get(entity);

  auto bound = transform.world() * particlesystem->bound;

  return storage->add(entity, bound, particlesystem, flags);
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<ParticleSystemComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  system<ParticleSystemComponentStorage>()->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<ParticleSystemComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  return system<ParticleSystemComponentStorage>()->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
ParticleSystemComponent Scene::get_component<ParticleSystemComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<ParticleSystemComponentStorage>()->get(entity);
}
