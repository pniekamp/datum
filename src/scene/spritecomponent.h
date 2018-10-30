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
    SpriteComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class SpriteComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

  protected:

    auto &entity(size_t index) const { return data<0>(index); }
    auto &flags(size_t index) const { return data<1>(index); }
    auto &sprite(size_t index) const { return data<2>(index); }
    auto &size(size_t index) const { return data<3>(index); }
    auto &layer(size_t index) const { return data<4>(index); }
    auto &tint(size_t index) const { return data<5>(index); }

    void set_entity(size_t index, Scene::EntityId entity) { data<0>(index) = entity; }
    void set_flags(size_t index, int flags) { data<1>(index) = flags; }
    void set_sprite(size_t index, Sprite const *sprite) { data<2>(index) = sprite; }
    void set_size(size_t index, float size) { data<3>(index) = size; }
    void set_layer(size_t index, float layer) { data<4>(index) = layer; }
    void set_tint(size_t index, lml::Color4 const &tint) { data<5>(index) = tint; }

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

    long flags() const { return storage->flags(index); }

    Sprite const *sprite() const { return storage->sprite(index); }

    float size() const { return storage->size(index); }
    float layer() const { return storage->layer(index); }
    lml::Color4 const &tint() const { return storage->tint(index); }

    lml::Rect2 bound() const { return lml::Rect2(-sprite()->pivot, lml::Vec2(size() * sprite()->aspect, size()) - sprite()->pivot); }

    void set_size(float size);
    void set_layer(float layer);
    void set_sprite(Sprite const *sprite, float size);
    void set_sprite(Sprite const *sprite, float size, lml::Color4 const &tint);
    void set_tint(lml::Color4 const &tint);

  private:

    size_t index;
    SpriteComponentStorage *storage;
};
