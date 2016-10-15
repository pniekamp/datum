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

class NameComponentStorage : public DefaultStorage<Scene::EntityId, size_t>
{
  public:
    NameComponentStorage(Scene *scene, StackAllocator<> allocator);

    const char *name(EntityId entity) const { return m_names.data() + data<1>(index(entity)); }

    // linear search
    EntityId find(const char *name) const;

  protected:

    std::vector<char, StackAllocator<>> m_names;
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
