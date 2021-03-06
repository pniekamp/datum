//
// Datum - sprite component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "spritecomponent.h"
#include "transformcomponent.h"
#include "debug.h"

using namespace std;
using namespace lml;

//|---------------------- SpriteStorage -------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// SpriteStorage::Constructor ////////////////////////
SpriteComponentStorage::SpriteComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator)
{
}


///////////////////////// SpriteStorage::Constructor ////////////////////////
void SpriteComponentStorage::add(EntityId entity, Sprite const *sprite, float size, float layer, Color4 tint, int flags)
{
  auto index = insert(entity);

  set_entity(index, entity);
  set_flags(index, flags);
  set_sprite(index, sprite);
  set_size(index,  size);
  set_layer(index, layer);
  set_tint(index, tint);
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<SpriteComponent>()
{
  m_systems[typeid(SpriteComponentStorage)] = new(allocate<SpriteComponentStorage>(m_allocator)) SpriteComponentStorage(this, m_allocator);
}


//|---------------------- SpriteComponent -----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// SpriteComponent::Constructor //////////////////////
SpriteComponent::SpriteComponent(size_t index, SpriteComponentStorage *storage)
  : index(index),
    storage(storage)
{
}


///////////////////////// SpriteComponent::set_size /////////////////////////
void SpriteComponent::set_size(float size)
{
  storage->set_size(index, size);
}


///////////////////////// SpriteComponent::set_layer ////////////////////////
void SpriteComponent::set_layer(float layer)
{
  storage->set_layer(index, layer);
}


///////////////////////// SpriteComponent::set_sprite ///////////////////////
void SpriteComponent::set_sprite(Sprite const *sprite, float size)
{
  storage->set_sprite(index, sprite);
  storage->set_size(index, size);
}


///////////////////////// SpriteComponent::set_sprite ///////////////////////
void SpriteComponent::set_sprite(Sprite const *sprite, float size, Color4 const &tint)
{
  storage->set_sprite(index, sprite);
  storage->set_size(index, size);
  storage->set_tint(index, tint);
}


///////////////////////// SpriteComponent::set_tint /////////////////////////
void SpriteComponent::set_tint(Color4 const &tint)
{
  storage->set_tint(index, tint);
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, Color4 tint, int flags)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  auto storage = system<SpriteComponentStorage>();

  storage->add(entity, sprite, size, 0.0f, tint, flags);

  return { storage->index(entity), storage };
}

template<>
SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, Color4 tint, SpriteComponent::Flags flags)
{
  return add_component<SpriteComponent>(entity, sprite, size, tint, (int)flags);
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, int flags)
{
  return add_component<SpriteComponent>(entity, sprite, size, Color4(1.0f, 1.0f, 1.0f, 1.0f), flags);
}

template<>
SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, SpriteComponent::Flags flags)
{
  return add_component<SpriteComponent>(entity, sprite, size, (int)flags);
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<SpriteComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  system<SpriteComponentStorage>()->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<SpriteComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  return system<SpriteComponentStorage>()->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
SpriteComponent Scene::get_component<SpriteComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<SpriteComponentStorage>()->get(entity);
}
