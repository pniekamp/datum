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

    auto &flags(size_t index) { return std::get<spriteflags>(m_data)[index]; }
    auto &sprite(size_t index) { return std::get<spriteresource>(m_data)[index]; }
    auto &size(size_t index) { return std::get<spritesize>(m_data)[index]; }
    auto &tint(size_t index) { return std::get<spritetint>(m_data)[index]; }

  public:

    void add(EntityId entity, Sprite const *sprite, float size, Color4 tint, long flags);
    void remove(EntityId entity);

  private:
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
void SpriteStoragePrivate::add(EntityId entity, Sprite const *sprite, float size, Color4 tint, long flags)
{
  DefaultStorage::add(entity);

  auto index = this->index(entity);

  get<spriteflags>(m_data)[index] = flags;
  get<spriteresource>(m_data)[index] = sprite;
  get<spritesize>(m_data)[index] = size;
  get<spritetint>(m_data)[index] = tint;
}


///////////////////////// SpriteStorage::remove /////////////////////////////
void SpriteStoragePrivate::remove(EntityId entity)
{
  DefaultStorage::remove(entity);
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<SpriteComponent>()
{
  m_systems[typeid(SpriteComponentStorage)] = new(allocator<SpriteStoragePrivate>().allocate(1)) SpriteStoragePrivate(this, allocator());
}



//|---------------------- SpriteComponent -----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Scene::add_component //////////////////////////////
template<>
SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, Color4 tint, long flags)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  auto storage = static_cast<SpriteStoragePrivate*>(system<SpriteComponentStorage>());

  storage->add(entity, sprite, size, tint, flags);

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

  auto storage = static_cast<SpriteStoragePrivate*>(system<SpriteComponentStorage>());

  storage->remove(entity);
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

  auto storage = static_cast<SpriteStoragePrivate*>(system<SpriteComponentStorage>());

  return { storage->index(entity), storage };
}


///////////////////////// SpriteComponent::Constructor ////////////////////////
SpriteComponent::SpriteComponent(size_t index, SpriteComponentStorage *storage)
  : index(index),
    storage(storage)
{
}
