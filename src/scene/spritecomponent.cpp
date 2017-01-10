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
SpriteComponentData *SpriteComponentStorage::add(EntityId entity, Sprite const *sprite, float size, float layer, lml::Color4 tint, long flags)
{
  auto data = BasicComponentStorage<SpriteComponentData>::add(entity);

  data->flags = flags;
  data->sprite = sprite;
  data->size = size;
  data->layer = layer;
  data->tint = tint;

  return data;
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<SpriteComponent>()
{
  m_systems[typeid(SpriteComponentStorage)] = new(allocator<SpriteComponentStorage>().allocate(1)) SpriteComponentStorage(this, allocator());
}


//|---------------------- SpriteComponent -----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// SpriteComponent::Constructor //////////////////////
SpriteComponent::SpriteComponent(SpriteComponentData *data)
  : m_data(data)
{
}


///////////////////////// SpriteComponent::set_size /////////////////////////
void SpriteComponent::set_size(float size)
{
  m_data->size = size;
}


///////////////////////// SpriteComponent::set_layer ////////////////////////
void SpriteComponent::set_layer(float layer)
{
  m_data->layer = layer;
}


///////////////////////// SpriteComponent::set_sprite ///////////////////////
void SpriteComponent::set_sprite(Sprite const *sprite, float size)
{
  m_data->sprite = sprite;
  m_data->size = size;
}


///////////////////////// SpriteComponent::set_sprite ///////////////////////
void SpriteComponent::set_sprite(Sprite const *sprite, float size, Color4 const &tint)
{
  m_data->sprite = sprite;
  m_data->size = size;
  m_data->tint = tint;
}


///////////////////////// SpriteComponent::set_tint /////////////////////////
void SpriteComponent::set_tint(Color4 const &tint)
{
  m_data->tint = tint;
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, Color4 tint, long flags)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  return system<SpriteComponentStorage>()->add(entity, sprite, size, 0.0f, tint, flags);
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, long flags)
{
  return add_component<SpriteComponent>(entity, sprite, size, Color4(1.0f, 1.0f, 1.0f, 1.0f), flags);
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
