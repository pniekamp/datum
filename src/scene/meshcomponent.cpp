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

///////////////////////// MeshStorage::Constructor //////////////////////////
MeshComponentStorage::MeshComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator),
    m_tree(StackAllocatorWithFreelist<>(allocator, m_treefreelist))
{
  m_staticpartition = 1;
}


///////////////////////// MeshStorage::clear ////////////////////////////////
void MeshComponentStorage::clear()
{
  m_tree.clear();

  m_staticpartition = 1;

  DefaultStorage::clear();
}


///////////////////////// MeshStorage::add //////////////////////////////////
void MeshComponentStorage::add(EntityId entity, Bound3 const &bound, Mesh const *mesh, Material const *material, int flags)
{
  size_t index = 0;

  if (flags & MeshComponent::Static)
  {
    index = insert(entity);

    if (index > m_staticpartition)
    {
      for_each(m_data, [=](auto &v) { swap(v[index], v[m_staticpartition]); });

      swap(m_index[data<0>(index).index()], m_index[entity.index()]);

      index = m_staticpartition;
    }

    m_staticpartition = max(m_staticpartition, index + 1);
  }
  else
  {
    index = append(entity);
  }

  set_entity(index, entity);
  set_flags(index, flags);
  set_bound(index, bound);
  set_mesh(index, mesh);
  set_material(index, material);

  if (flags & MeshComponent::Static)
  {
    m_tree.insert(MeshIndex{ index, this });
  }
}


///////////////////////// MeshStorage::remove ///////////////////////////////
void MeshComponentStorage::remove(EntityId entity)
{
  auto index = m_index[entity.index()];

  if (index < m_staticpartition)
  {
    m_tree.remove(MeshIndex{ index, this });

    for_each(m_data, [=](auto &v) { v[index] = {}; });

    m_freeslots.push_back(index);
  }
  else
  {
    for_each(m_data, [=](auto &v) { swap(v[index], v[v.size()-1]); });

    for_each(m_data, [](auto &v) { v.resize(v.size()-1); });

    m_index[data<0>(index).index()] = index;
  }

  m_index[entity.index()] = 0;
}


///////////////////////// MeshStorage::update_mesh_bounds ///////////////////
void MeshComponentStorage::update_mesh_bounds()
{
  auto transformstorage = m_scene->system<TransformComponentStorage>();

  for(size_t index = m_staticpartition; index < size(); ++index)
  {
    assert(transformstorage->has(entity(index)));

    auto transform = transformstorage->get(entity(index));

    set_bound(index, transform.world() * mesh(index)->bound);
  }
}


///////////////////////// update_meshes /////////////////////////////////////
void update_meshes(Scene &scene)
{
  auto meshstorage = scene.system<MeshComponentStorage>();

  meshstorage->update_mesh_bounds();
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<MeshComponent>()
{
  m_systems[typeid(MeshComponentStorage)] = new(allocate<MeshComponentStorage>(m_allocator)) MeshComponentStorage(this, m_allocator);
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
MeshComponent Scene::add_component<MeshComponent>(Scene::EntityId entity, Mesh const *mesh, Material const *material, int flags)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));
  assert(mesh);

  auto storage = system<MeshComponentStorage>();

  auto transform = system<TransformComponentStorage>()->get(entity);

  auto bound = transform.world() * mesh->bound;

  storage->add(entity, bound, mesh, material, flags);

  return { storage->index(entity), storage };
}

template<>
MeshComponent Scene::add_component<MeshComponent>(Scene::EntityId entity, Mesh const *mesh, Material const *material, MeshComponent::Flags flags)
{
  return add_component<MeshComponent>(entity, mesh, material, (int)flags);
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<MeshComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<MeshComponentStorage>()->remove(entity);
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
