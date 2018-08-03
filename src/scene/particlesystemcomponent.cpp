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

///////////////////////// ParticleSystemStorage::Constructor ////////////////
ParticleSystemComponentStorage::ParticleSystemComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator),
    m_allocator(allocator, m_freelist)
{
}


///////////////////////// ParticleSystemStorage::add ////////////////////////
void ParticleSystemComponentStorage::add(EntityId entity, Bound3 const &bound, ParticleSystem const *system, int flags)
{
  auto index = insert(entity);

  set_entity(index, entity);
  set_flags(index, flags);
  set_bound(index, bound);
  set_system(index, system);
  set_instance(index, system->create(m_allocator));
}


///////////////////////// ParticleSystemStorage::remove /////////////////////
void ParticleSystemComponentStorage::remove(EntityId entity)
{
  auto index = this->index(entity);

  system(index)->destroy(instance(index));

  DefaultStorage::remove(entity);
}


///////////////////////// update_particlesystem_bounds //////////////////////
void ParticleSystemComponentStorage::update_particlesystem_bounds()
{
  auto transformstorage = m_scene->system<TransformComponentStorage>();

  for(size_t index = 1; index < size(); ++index)
  {
    if (entity(index))
    {
      assert(transformstorage->has(entity(index)));

      auto transform = transformstorage->get(entity(index));

      set_bound(index, transform.world() * system(index)->bound);
    }
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

  particlestorage->update_particlesystem_bounds();
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<ParticleSystemComponent>()
{
  m_systems[typeid(ParticleSystemComponentStorage)] = new(allocate<ParticleSystemComponentStorage>(m_allocator)) ParticleSystemComponentStorage(this, m_allocator);
}



//|---------------------- ParticleSystemComponent ---------------------------
//|--------------------------------------------------------------------------

///////////////////////// ParticleSystemComponent::Constructor //////////////
ParticleSystemComponent::ParticleSystemComponent(size_t index, ParticleSystemComponentStorage *storage)
  : index(index),
    storage(storage)
{
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
ParticleSystemComponent Scene::add_component<ParticleSystemComponent>(Scene::EntityId entity, ParticleSystem const *particlesystem, int flags)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));
  assert(particlesystem);

  auto storage = system<ParticleSystemComponentStorage>();

  auto transform = system<TransformComponentStorage>()->get(entity);

  auto bound = transform.world() * particlesystem->bound;

  storage->add(entity, bound, particlesystem, flags);

  return { storage->index(entity), storage };
}

template<>
ParticleSystemComponent Scene::add_component<ParticleSystemComponent>(Scene::EntityId entity, ParticleSystem const *particlesystem, ParticleSystemComponent::Flags flags)
{
  return add_component<ParticleSystemComponent>(entity, particlesystem, (int)flags);
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
