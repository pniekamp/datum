//
// Datum - mesh component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "meshcomponent.h"
#include "debug.h"

using namespace std;
using namespace lml;

//|---------------------- MeshStorage ---------------------------------------
//|--------------------------------------------------------------------------

class MeshStoragePrivate : public MeshComponentStorage
{
  public:

    typedef StackAllocator<> allocator_type;

    MeshStoragePrivate(Scene *scene, allocator_type const &allocator);

  public:

    auto &flags(size_t index) { return data<meshflags>(index); }
    auto &bound(size_t index) { return data<boundingbox>(index); }
    auto &mesh(size_t index) { return data<meshresource>(index); }
    auto &material(size_t index) { return data<materialresource>(index); }

  public:

    void clear() override;

    void add(EntityId entity, Bound3 const &bound, Mesh const *mesh, Material const *material, long flags);

    void remove(EntityId entity) override;

    void update_mesh_bounds();

  public:

    size_t m_staticpartition;

    FreeList m_treefreelist;

    RTree::basic_rtree<MeshIndex, 3, RTree::box<MeshIndex>, StackAllocatorWithFreelist<>> m_tree;
};


///////////////////////// MeshStorage::Constructor //////////////////////////
MeshComponentStorage::MeshComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator)
{
}


///////////////////////// MeshStorage::Constructor //////////////////////////
MeshStoragePrivate::MeshStoragePrivate(Scene *scene, allocator_type const &allocator)
  : MeshComponentStorage(scene, allocator),
    m_tree(StackAllocatorWithFreelist<>(allocator, m_treefreelist))
{
  m_staticpartition = 1;
}


///////////////////////// MeshStorage::clear ////////////////////////////////
void MeshStoragePrivate::clear()
{
  m_tree.clear();

  m_staticpartition = 1;

  DefaultStorage::clear();
}


///////////////////////// MeshStorage::add //////////////////////////////////
void MeshStoragePrivate::add(EntityId entity, Bound3 const &bound, Mesh const *mesh, Material const *material, long flags)
{
  size_t index = size();

  m_index.resize(std::max(m_index.size(), entity.index()+1));

  if (flags & MeshComponent::Static)
  {
    if (m_freeslots.size() != 0)
    {
      index = m_freeslots.front();

      m_freeslots.pop_front();
    }
    else
    {
      for_each(m_data, [](auto &v) { v.resize(v.size()+1); });

      if (index != m_staticpartition)
      {
        for_each(m_data, [=](auto &v) { swap(v[index], v[m_staticpartition]); });

        m_index[data<entityid>(index).index()] = index;

        index = m_staticpartition;
      }

      m_staticpartition += 1;
    }
  }
  else
  {
    for_each(m_data, [](auto &v) { v.resize(v.size()+1); });
  }

  m_index[entity.index()] = index;

  data<meshflags>(index) = flags;
  data<entityid>(index) = entity;
  data<boundingbox>(index) = bound;
  data<meshresource>(index) = mesh;
  data<materialresource>(index) = material;

  if (flags & MeshComponent::Static)
  {
    m_tree.insert(MeshIndex{ index, this });
  }
}


///////////////////////// MeshStorage::remove ///////////////////////////////
void MeshStoragePrivate::remove(EntityId entity)
{
  auto index = m_index[entity.index()];

  if (index < m_staticpartition)
  {
    m_tree.remove(MeshIndex{ index, this });

    m_freeslots.push_back(index);
  }
  else
  {
    for_each(m_data, [=](auto &v) { swap(v[index], v[v.size()-1]); });

    for_each(m_data, [](auto &v) { v.resize(v.size()-1); });

    m_index[data<entityid>(index).index()] = index;
  }

  m_index[entity.index()] = 0;
}


///////////////////////// MeshStorage::update_mesh_bounds ///////////////////
void MeshStoragePrivate::update_mesh_bounds()
{
  auto transformstorage = m_scene->system<TransformComponentStorage>();

  for(size_t index = m_staticpartition; index < size(); ++index)
  {
    assert(transformstorage->has(data<entityid>(index)));

    auto transform = transformstorage->get(data<entityid>(index));

    data<boundingbox>(index) = transform.world() * mesh(index)->bound;
  }
}


///////////////////////// update_mesh_bounds ////////////////////////////////
void update_mesh_bounds(Scene &scene)
{
  auto storage = static_cast<MeshStoragePrivate*>(scene.system<MeshComponentStorage>());

  storage->update_mesh_bounds();
}


///////////////////////// MeshStorage::tree /////////////////////////////////
MeshComponentStorage::iterator_pair<MeshComponentStorage::tree_iterator> MeshComponentStorage::tree() const
{
  auto storage = static_cast<MeshStoragePrivate const*>(this);

  return { storage->m_tree.begin(), storage->m_tree.end() };
}


///////////////////////// MeshStorage::dynamic //////////////////////////////
MeshComponentStorage::iterator_pair<Scene::EntityId const *> MeshComponentStorage::dynamic() const
{
  auto storage = static_cast<MeshStoragePrivate const*>(this);

  return { std::get<entityid>(m_data).data() + storage->m_staticpartition, std::get<entityid>(m_data).data() + size() };
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<MeshComponent>()
{
  m_systems[typeid(MeshComponentStorage)] = new(allocator<MeshStoragePrivate>().allocate(1)) MeshStoragePrivate(this, allocator());
}



//|---------------------- MeshComponent -------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// MeshComponent::Constructor ////////////////////////
MeshComponent::MeshComponent(size_t index, MeshComponentStorage *storage)
  : index(index),
    storage(storage)
{
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
MeshComponent Scene::add_component<MeshComponent>(Scene::EntityId entity, Mesh const *mesh, Material const *material, long flags)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  auto storage = static_cast<MeshStoragePrivate*>(system<MeshComponentStorage>());

  auto transform = system<TransformComponentStorage>()->get(entity);

  auto bound = transform.world() * mesh->bound;

  storage->add(entity, bound, mesh, material, flags);

  return { storage->index(entity), storage };
}

template<>
MeshComponent Scene::add_component<MeshComponent>(Scene::EntityId entity, Mesh const *mesh, Material const *material, int flags)
{
  return add_component<MeshComponent>(entity, mesh, material, (long)flags);
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<MeshComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return static_cast<MeshStoragePrivate*>(system<MeshComponentStorage>())->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<MeshComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  return system<MeshComponentStorage>()->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
MeshComponent Scene::get_component<MeshComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<MeshComponentStorage>()->get(entity);
}
