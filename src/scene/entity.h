//
// Datum - entity
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "scene.h"


//|---------------------- Entity --------------------------------------------
//|--------------------------------------------------------------------------

class Entity
{
  public:
    friend Scene::EntityId Scene::create<Entity>();

  protected:
    Entity();
    virtual ~Entity();

    virtual std::type_info const &type() const
    {
      return typeid(*this);
    }

  private:

    friend class Scene;
};

