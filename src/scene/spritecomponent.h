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

//|---------------------- SpriteComponentStorage ----------------------------
//|--------------------------------------------------------------------------

class SpriteComponentStorage  : public DefaultStorage<Scene::EntityId, int, Sprite const *, float, float, lml::Color4>
{
  public:

    enum DataLayout
    {
      entityid = 0,
      flagbits = 1,
      spriteresource = 2,
      spritesize  = 3,
      spritelayer = 4,
      spritetint = 5,
    };

  public:
    SpriteComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class SpriteComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

  protected:

    void add(EntityId entity, Sprite const *sprite, float size, float layer, lml::Color4 tint, int flags);

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
    friend SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, lml::Color4 tint, int flags);
    friend SpriteComponent Scene::add_component<SpriteComponent>(Scene::EntityId entity, Sprite const *sprite, float size, int flags);
    friend SpriteComponent Scene::get_component<SpriteComponent>(Scene::EntityId entity);

  public:
    SpriteComponent() = default;
    SpriteComponent(size_t index, SpriteComponentStorage *storage);

    long flags() const { return storage->data<SpriteComponentStorage::flagbits>(index); }

    Sprite const *sprite() const { return storage->data<SpriteComponentStorage::spriteresource>(index); }

    float size() const { return storage->data<SpriteComponentStorage::spritesize>(index); }
    float layer() const { return storage->data<SpriteComponentStorage::spritelayer>(index); }
    lml::Color4 const &tint() const { return storage->data<SpriteComponentStorage::spritetint>(index); }

    lml::Rect2 bound() const { return lml::Rect2(-sprite()->align, lml::Vec2(size() * sprite()->aspect, size()) - sprite()->align); }

    void set_size(float size);
    void set_layer(float layer);
    void set_sprite(Sprite const *sprite, float size);
    void set_sprite(Sprite const *sprite, float size, lml::Color4 const &tint);
    void set_tint(lml::Color4 const &tint);

  private:

    size_t index;
    SpriteComponentStorage *storage;
};
