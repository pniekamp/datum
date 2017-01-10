//
// Datum - sprite component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "scene.h"
#include "basiccomponent.h"
#include "datum/math.h"
#include "datum/renderer.h"

//|---------------------- SpriteComponentStorage ----------------------------
//|--------------------------------------------------------------------------

struct SpriteComponentData
{
  long flags;
  Sprite const *sprite;
  float size;
  float layer;
  lml::Color4 tint;
};

class SpriteComponentStorage : public BasicComponentStorage<SpriteComponentData>
{
  public:
    using BasicComponentStorage::BasicComponentStorage;

    template<typename Component = class SpriteComponent>
    Component get(EntityId entity)
    {
      return data(entity);
    }

  protected:

    SpriteComponentData *add(EntityId entity, Sprite const *sprite, float size, float layer, lml::Color4 tint, long flags);

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
    SpriteComponent(SpriteComponentData *data);

    long flags() const { return m_data->flags; }

    Sprite const *sprite() const { return m_data->sprite; }

    float size() const { return m_data->size; }
    float layer() const { return m_data->layer; }
    lml::Color4 const &tint() const { return m_data->tint; }

    lml::Rect2 bound() const { return lml::Rect2(-sprite()->align, lml::Vec2(size() * sprite()->aspect, size()) - sprite()->align); }

    void set_size(float size);
    void set_layer(float layer);
    void set_sprite(Sprite const *sprite, float size);
    void set_sprite(Sprite const *sprite, float size, lml::Color4 const &tint);
    void set_tint(lml::Color4 const &tint);

  private:

    SpriteComponentData *m_data;
};
