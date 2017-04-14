//
// Datum - mesh component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "scene.h"
#include "storage.h"
#include "transformcomponent.h"
#include "datum/math.h"
#include "datum/renderer.h"
#include <leap/lml/rtree.h>

//|---------------------- ActorComponentStorage -----------------------------
//|--------------------------------------------------------------------------

class ActorComponentStorage : public DefaultStorage<Scene::EntityId, int, lml::Bound3, Mesh const *, Material const *, Animator *>
{
  public:

    enum DataLayout
    {
      entityid = 0,
      flagbits = 1,
      boundingbox = 2,
      meshresource = 3,
      materialresource = 4,
      animator = 5,
    };

  public:
    ActorComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class ActorComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

    void update_mesh_bounds();

  public:

    struct MeshIndex
    {
      operator EntityId() const
      {
        return storage->data<entityid>(index);
      }

      lml::Bound3 const &box() const
      {
        return storage->data<boundingbox>(index);
      }

      friend bool operator ==(MeshIndex const &lhs, MeshIndex const &rhs)
      {
        return lhs.index == rhs.index;
      }

      size_t index;
      ActorComponentStorage *storage;
    };

    template<typename Component = class ActorComponent>
    Component get(MeshIndex const &index)
    {
      return { index.index, this };
    }

    typedef leap::lml::RTree::basic_rtree<MeshIndex, 3, leap::lml::RTree::box<MeshIndex>, StackAllocatorWithFreelist<>>::const_iterator tree_iterator;

    iterator_pair<tree_iterator> tree() const
    {
      return { m_tree.begin(), m_tree.end() };
    }

    iterator_pair<EntityId const *> dynamic() const
    {
      return { std::get<entityid>(m_data).data() + m_staticpartition, std::get<entityid>(m_data).data() + size() };
    }

  protected:

    void clear() override;

    void add(EntityId entity, lml::Bound3 const &bound, Mesh const *mesh, Material const *material, int flags);

    void remove(EntityId entity) override;

  protected:

    FreeList m_freelist;
    StackAllocatorWithFreelist<> m_allocator;

  protected:

    size_t m_staticpartition;

    FreeList m_treefreelist;

    leap::lml::RTree::basic_rtree<MeshIndex, 3, leap::lml::RTree::box<MeshIndex>, StackAllocatorWithFreelist<>> m_tree;

    friend class Scene;
    friend class ActorComponent;
};


///////////////////////// update_actors /////////////////////////////////////
void update_actors(Scene &scene, Camera const &camera, float dt);


//|---------------------- ActorComponent ------------------------------------
//|--------------------------------------------------------------------------

class ActorComponent
{
  public:

    enum Flags
    {
      Visible = 0x01,
      Static = 0x02,
    };

  public:
    friend ActorComponent Scene::add_component<ActorComponent>(Scene::EntityId entity, Mesh const *mesh, Material const *material, int flags);
    friend ActorComponent Scene::get_component<ActorComponent>(Scene::EntityId entity);

  public:
    ActorComponent() = default;
    ActorComponent(size_t index, ActorComponentStorage *storage);

    int flags() const { return storage->data<ActorComponentStorage::flagbits>(index); }

    lml::Bound3 const &bound() const { return storage->data<ActorComponentStorage::boundingbox>(index); }

    Mesh const *mesh() const { return storage->data<ActorComponentStorage::meshresource>(index); }
    Material const *material() const { return storage->data<ActorComponentStorage::materialresource>(index); }

    Pose const &pose() const { return animator()->pose; }

    Animator *animator() const { return storage->data<ActorComponentStorage::animator>(index); }

  protected:

    size_t index;
    ActorComponentStorage *storage;
};
