//
// Datum - sprite component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "scene.h"
#include "storage.h"
#include "datum/math.h"
#include "datum/renderer.h"


//|---------------------- SpriteComponentStorage ---------------------------
//|--------------------------------------------------------------------------

class SpriteComponentStorage : public DefaultStorage<long, Sprite const *, float, lml::Color4>
{
  public:

    enum DataLayout
    {
      spriteflags = 0,
      spriteresource = 1,
      spritesize = 2,
      spritetint = 3,
    };

  public:
    SpriteComponentStorage(Scene *scene, StackAllocator<> allocator);

    long const &flags(EntityId entity) const { return data<spriteflags>(index(entity)); }

    Sprite const *sprite(EntityId entity) const { return data<spriteresource>(index(entity)); }

    float const &size(EntityId entity) const { return data<spritesize>(index(entity)); }

    lml::Color4 const &tint(EntityId entity) const { return data<spritetint>(index(entity)); }
};



//|---------------------- SpriteComponent -----------------------------------
//|--------------------------------------------------------------------------

class SpriteComponent
{
  public:

    enum Flags
    {
      Visible = 0x01,
    };

  public:
    friend SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, lml::Color4 tint, long flags);
    friend SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, long flags);
    friend SpriteComponent Scene::get_component<SpriteComponent>(Scene::EntityId entity);

  public:

    long const &flags() const { return storage->data<SpriteComponentStorage::spriteflags>(index); }

    Sprite const *sprite() const { return storage->data<SpriteComponentStorage::spriteresource>(index); }

    float const &size() const { return storage->data<SpriteComponentStorage::spritesize>(index); }

    lml::Color4 const &tint() const { return storage->data<SpriteComponentStorage::spritetint>(index); }

    lml::Rect2 bound() const { return lml::Rect2(-sprite()->align, lml::Vec2(size() * sprite()->aspect, size()) - sprite()->align); }

  protected:
    SpriteComponent(size_t index, SpriteComponentStorage *storage);

    size_t index;
    SpriteComponentStorage *storage;
};
