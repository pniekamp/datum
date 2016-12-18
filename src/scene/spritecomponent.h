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

class SpriteComponentStorage : public DefaultStorage<long, Sprite const *, float, float, lml::Color4>
{
  public:

    enum DataLayout
    {
      spriteflags = 0,
      spriteresource = 1,
      spritesize = 2,
      spritelayer = 3,
      spritetint = 4,
    };

  public:
    SpriteComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class SpriteComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

  protected:

    void add(EntityId entity, Sprite const *sprite, float size, float layer, lml::Color4 const &tint, long flags);

    void set_layer(size_t index, float layer);

    void set_sprite(size_t index, Sprite const *sprite, float size, lml::Color4 const &tint);

    friend class Scene;
    friend class SpriteComponent;
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
    SpriteComponent(size_t index, SpriteComponentStorage *storage);

    long const &flags() const { return storage->data<SpriteComponentStorage::spriteflags>(index); }

    Sprite const *sprite() const { return storage->data<SpriteComponentStorage::spriteresource>(index); }

    float const &size() const { return storage->data<SpriteComponentStorage::spritesize>(index); }
    float const &layer() const { return storage->data<SpriteComponentStorage::spritelayer>(index); }
    lml::Color4 const &tint() const { return storage->data<SpriteComponentStorage::spritetint>(index); }

    lml::Rect2 bound() const { return lml::Rect2(-sprite()->align, lml::Vec2(size() * sprite()->aspect, size()) - sprite()->align); }

    void set_size(float size);
    void set_layer(float layer);
    void set_sprite(Sprite const *sprite, float size);
    void set_sprite(Sprite const *sprite, float size, lml::Color4 const &tint);
    void set_tint(lml::Color4 const &tint);

  protected:

    size_t index;
    SpriteComponentStorage *storage;
};
