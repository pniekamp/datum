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

class SpriteStoragePrivate : public SpriteComponentStorage
{
  public:

    typedef StackAllocator<> allocator_type;

    SpriteStoragePrivate(Scene *scene, allocator_type const &allocator);

  public:

    auto &flags(size_t index) { return data<spriteflags>(index); }
    auto &sprite(size_t index) { return data<spriteresource>(index); }
    auto &size(size_t index) { return data<spritesize>(index); }
    auto &layer(size_t index) { return data<spritelayer>(index); }
    auto &tint(size_t index) { return data<spritetint>(index); }

  public:

    void add(EntityId entity, Sprite const *sprite, float size, float layer, Color4 const &tint, long flags);

    void set_layer(size_t index, float layer);

    void set_sprite(size_t index, Sprite const *sprite, float size, Color4 const &tint);
};


///////////////////////// SpriteStorage::Constructor ////////////////////////
SpriteComponentStorage::SpriteComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator)
{
}


///////////////////////// SpriteStorage::Constructor ////////////////////////
SpriteStoragePrivate::SpriteStoragePrivate(Scene *scene, allocator_type const &allocator)
  : SpriteComponentStorage(scene, allocator)
{
}


///////////////////////// SpriteStorage::add ////////////////////////////////
void SpriteStoragePrivate::add(EntityId entity, Sprite const *sprite, float size, float layer, Color4 const &tint, long flags)
{
  DefaultStorage::add(entity);

  auto index = this->index(entity);

  data<spriteflags>(index) = flags;
  data<spriteresource>(index) = sprite;
  data<spritesize>(index) = size;
  data<spritelayer>(index) = layer;
  data<spritetint>(index) = tint;
}


///////////////////////// SpriteStorage::set_layer //////////////////////////
void SpriteStoragePrivate::set_layer(size_t index, float layer)
{
  data<spritelayer>(index) = layer;
}


///////////////////////// SpriteStorage::set_sprite /////////////////////////
void SpriteStoragePrivate::set_sprite(size_t index, Sprite const *sprite, float size, Color4 const &tint)
{
  data<spriteresource>(index) = sprite;
  data<spritesize>(index) = size;
  data<spritetint>(index) = tint;
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<SpriteComponent>()
{
  m_systems[typeid(SpriteComponentStorage)] = new(allocator<SpriteStoragePrivate>().allocate(1)) SpriteStoragePrivate(this, allocator());
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
  auto storage = static_cast<SpriteStoragePrivate*>(this->storage);

  storage->set_sprite(index, sprite(), size, tint());
}


///////////////////////// SpriteComponent::set_layer ////////////////////////
void SpriteComponent::set_layer(float layer)
{
  auto storage = static_cast<SpriteStoragePrivate*>(this->storage);

  storage->set_layer(index, layer);
}


///////////////////////// SpriteComponent::set_sprite ///////////////////////
void SpriteComponent::set_sprite(Sprite const *sprite, float size)
{
  auto storage = static_cast<SpriteStoragePrivate*>(this->storage);

  storage->set_sprite(index, sprite, size, tint());
}


///////////////////////// SpriteComponent::set_sprite ///////////////////////
void SpriteComponent::set_sprite(Sprite const *sprite, float size, Color4 const &tint)
{
  auto storage = static_cast<SpriteStoragePrivate*>(this->storage);

  storage->set_sprite(index, sprite, size, tint);
}


///////////////////////// SpriteComponent::set_tint /////////////////////////
void SpriteComponent::set_tint(Color4 const &tint)
{
  auto storage = static_cast<SpriteStoragePrivate*>(this->storage);

  storage->set_sprite(index, sprite(), size(), tint);
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, Color4 tint, long flags)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  auto storage = static_cast<SpriteStoragePrivate*>(system<SpriteComponentStorage>());

  storage->add(entity, sprite, size, 0.0f, tint, flags);

  return { storage->index(entity), storage };
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

  static_cast<SpriteStoragePrivate*>(system<SpriteComponentStorage>())->remove(entity);
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
