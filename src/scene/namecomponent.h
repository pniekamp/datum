//
// Datum - name component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "scene.h"
#include "storage.h"


//|---------------------- NameComponentStorage ------------------------------
//|--------------------------------------------------------------------------

class NameComponentStorage : public DefaultStorage<char>
{
  public:
    NameComponentStorage(Scene *scene, StackAllocatorWithFreelist<> allocator);

    const char *name(EntityId entity) const { return &data<0>(index(entity)); }

    // linear search
    EntityId find(const char *name) const;
};



//|---------------------- NameComponent -------------------------------------
//|--------------------------------------------------------------------------

class NameComponent
{
  public:
    friend NameComponent Scene::add_component<NameComponent>(Scene::EntityId entity, const char *name);
    friend NameComponent Scene::get_component<NameComponent>(Scene::EntityId entity);

  public:

    const char *name() const { return storage->name(entity); }

    void set_name(const char *name);

  private:
    NameComponent(Scene::EntityId entity, NameComponentStorage *storage);

    Scene::EntityId entity;
    NameComponentStorage *storage;
};
