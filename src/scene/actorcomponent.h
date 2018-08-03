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
        return storage->data<0>(index);
      }

      lml::Bound3 const &box() const
      {
        return storage->data<2>(index);
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
      return { std::get<0>(m_data).data() + m_staticpartition, std::get<0>(m_data).data() + size() };
    }

  protected:

    auto &entity(size_t index) const { return data<0>(index); }
    auto &flags(size_t index) const { return data<1>(index); }
    auto &bound(size_t index) const { return data<2>(index); }
    auto &mesh(size_t index) const { return data<3>(index); }
    auto &material(size_t index) const { return data<4>(index); }
    auto &animator(size_t index) const { return data<5>(index); }

    void set_entity(size_t index, Scene::EntityId entity) { data<0>(index) = entity; }
    void set_flags(size_t index, int flags) { data<1>(index) = flags; }
    void set_bound(size_t index, lml::Bound3 const &bound) { data<2>(index) = bound; }
    void set_mesh(size_t index, Mesh const *mesh) { data<3>(index) = mesh; }
    void set_material(size_t index, Material const *material) { data<4>(index) = material; }
    void set_animator(size_t index, Animator *animator) { data<5>(index) = animator; }

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

    int flags() const { return storage->flags(index); }

    lml::Bound3 const &bound() const { return storage->bound(index); }

    Mesh const *mesh() const { return storage->mesh(index); }
    Material const *material() const { return storage->material(index); }

    Animator *animator() const { return storage->animator(index); }

    Pose const &pose() const { return storage->animator(index)->pose; }

  protected:

    size_t index;
    ActorComponentStorage *storage;
};
