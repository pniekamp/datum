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

//|---------------------- MeshComponentStorage ------------------------------
//|--------------------------------------------------------------------------

class MeshComponentStorage : public DefaultStorage<int, Scene::EntityId, lml::Bound3, Mesh const *, Material const *>
{
  public:

    enum DataLayout
    {
      flagbits = 0,
      entityid = 1,
      boundingbox = 2,
      meshresource = 3,
      materialresource = 4,
    };

  public:
    MeshComponentStorage(Scene *scene, StackAllocator<> allocator);

    template<typename Component = class MeshComponent>
    Component get(EntityId entity)
    {
      return { this->index(entity), this };
    }

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
      MeshComponentStorage *storage;
    };

    template<typename Component = class MeshComponent>
    Component get(MeshIndex const &index)
    {
      return { index.index, this };
    }

    template<typename Iterator>
    class iterator_pair : public std::pair<Iterator, Iterator>
    {
      public:
        using std::pair<Iterator, Iterator>::pair;

        Iterator begin() const { return this->first; }
        Iterator end() const { return this->second; }
    };

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

    auto &flags(size_t index) { return data<flagbits>(index); }
    auto &bound(size_t index) { return data<boundingbox>(index); }
    auto &mesh(size_t index) { return data<meshresource>(index); }
    auto &material(size_t index) { return data<materialresource>(index); }

  protected:

    void clear() override;

    void add(EntityId entity, lml::Bound3 const &bound, Mesh const *mesh, Material const *material, int flags);

    void remove(EntityId entity) override;

    void update_mesh_bounds();

  protected:

    size_t m_staticpartition;

    FreeList m_treefreelist;

    leap::lml::RTree::basic_rtree<MeshIndex, 3, leap::lml::RTree::box<MeshIndex>, StackAllocatorWithFreelist<>> m_tree;

    friend class Scene;
    friend class MeshComponent;
    friend void update_mesh_bounds(Scene &scene);
};


///////////////////////// update_mesh_bounds ////////////////////////////////
void update_mesh_bounds(Scene &scene);


//|---------------------- MeshComponent -------------------------------------
//|--------------------------------------------------------------------------

class MeshComponent
{
  public:

    enum Flags
    {
      Visible = 0x01,
      Static = 0x02,
    };

  public:
    friend MeshComponent Scene::add_component<MeshComponent>(Scene::EntityId entity, Mesh const *mesh, Material const *material, int flags);
    friend MeshComponent Scene::get_component<MeshComponent>(Scene::EntityId entity);

  public:
    MeshComponent() = default;
    MeshComponent(size_t index, MeshComponentStorage *storage);

    int flags() const { return storage->data<MeshComponentStorage::flagbits>(index); }

    lml::Bound3 const &bound() const { return storage->data<MeshComponentStorage::boundingbox>(index); }

    Mesh const *mesh() const { return storage->data<MeshComponentStorage::meshresource>(index); }
    Material const *material() const { return storage->data<MeshComponentStorage::materialresource>(index); }

  protected:

    size_t index;
    MeshComponentStorage *storage;
};
