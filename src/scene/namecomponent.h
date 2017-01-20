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

    template<typename Component = class NameComponent>
    Component get(EntityId entity)
    {
      return { entity, this };
    }

  protected:

    void clear() override;

    void add(EntityId entity, const char *name);

    void set_name(EntityId entity, const char *name);

    std::vector<char, StackAllocator<char>> m_names;

    friend class Scene;
    friend class NameComponent;
};


//|---------------------- NameComponent -------------------------------------
//|--------------------------------------------------------------------------

class NameComponent
{
  public:
    friend NameComponent Scene::add_component<NameComponent>(Scene::EntityId entity, const char *name);
    friend NameComponent Scene::get_component<NameComponent>(Scene::EntityId entity);

  public:
    NameComponent() = default;
    NameComponent(Scene::EntityId entity, NameComponentStorage *storage);

    const char *name() const { return storage->name(entity); }

    void set_name(const char *name);

  protected:

    Scene::EntityId entity;
    NameComponentStorage *storage;
};
