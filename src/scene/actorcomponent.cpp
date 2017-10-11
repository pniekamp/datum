//
// Datum - mesh component
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "actorcomponent.h"
#include "debug.h"

using namespace std;
using namespace lml;

//|---------------------- MeshStorage ---------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// MeshStorage::Constructor //////////////////////////
ActorComponentStorage::ActorComponentStorage(Scene *scene, StackAllocator<> allocator)
  : DefaultStorage(scene, allocator),
    m_allocator(allocator, m_freelist),
    m_tree(StackAllocatorWithFreelist<>(allocator, m_treefreelist))
{
  m_staticpartition = 1;
}


///////////////////////// MeshStorage::clear ////////////////////////////////
void ActorComponentStorage::clear()
{
  m_tree.clear();

  m_staticpartition = 1;

  DefaultStorage::clear();
}


///////////////////////// MeshStorage::add //////////////////////////////////
void ActorComponentStorage::add(EntityId entity, Bound3 const &bound, Mesh const *mesh, Material const *material, int flags)
{
  size_t index = size();

  m_index.resize(std::max(m_index.size(), entity.index()+1));

  if (flags & ActorComponent::Static)
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

  data<entityid>(index) = entity;
  data<flagbits>(index) = flags;
  data<boundingbox>(index) = bound;
  data<meshresource>(index) = mesh;
  data<materialresource>(index) = material;
  data<animator>(index) = new(allocate<Animator>(m_allocator)) Animator(m_allocator);
  data<animator>(index)->set_mesh(mesh);

  if (flags & ActorComponent::Static)
  {
    m_tree.insert(MeshIndex{ index, this });
  }
}


///////////////////////// MeshStorage::remove ///////////////////////////////
void ActorComponentStorage::remove(EntityId entity)
{
  auto index = m_index[entity.index()];

  data<animator>(index)->~Animator();
  deallocate<Animator>(m_allocator, data<animator>(index));

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

    m_index[data<entityid>(index).index()] = index;
  }

  m_index[entity.index()] = 0;
}


///////////////////////// MeshStorage::update_mesh_bounds ///////////////////
void ActorComponentStorage::update_mesh_bounds()
{
  auto transformstorage = m_scene->system<TransformComponentStorage>();

  for(size_t index = m_staticpartition; index < size(); ++index)
  {
    assert(transformstorage->has(data<entityid>(index)));

    auto transform = transformstorage->get(data<entityid>(index));

    data<boundingbox>(index) = transform.world() * data<meshresource>(index)->bound;
  }
}


///////////////////////// update_actors /////////////////////////////////////
void update_actors(Scene &scene, Camera const &camera, float dt)
{
  auto frustum = camera.frustum();

  auto actorstorage = scene.system<ActorComponentStorage>();

  for(auto &entity : actorstorage->entities())
  {
    auto actor = actorstorage->get(entity);

    if (intersects(frustum, actor.bound()))
    {
      actor.animator()->update(dt);
    }
  }

  actorstorage->update_mesh_bounds();
}


///////////////////////// Scene::initialise_storage ////////////////////////
template<>
void Scene::initialise_component_storage<ActorComponent>()
{
  m_systems[typeid(ActorComponentStorage)] = new(allocate<ActorComponentStorage>(m_allocator)) ActorComponentStorage(this, m_allocator);
}



//|---------------------- ActorComponent ------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ActorComponent::Constructor ///////////////////////
ActorComponent::ActorComponent(size_t index, ActorComponentStorage *storage)
  : index(index),
    storage(storage)
{
}


///////////////////////// Scene::add_component //////////////////////////////
template<>
ActorComponent Scene::add_component<ActorComponent>(Scene::EntityId entity, Mesh const *mesh, Material const *material, int flags)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));
  assert(mesh);

  auto storage = system<ActorComponentStorage>();

  auto transform = system<TransformComponentStorage>()->get(entity);

  auto bound = transform.world() * mesh->bound;

  storage->add(entity, bound, mesh, material, flags);

  return { storage->index(entity), storage };
}

template<>
ActorComponent Scene::add_component<ActorComponent>(Scene::EntityId entity, Mesh const *mesh, Material const *material, ActorComponent::Flags flags)
{
  return add_component<ActorComponent>(entity, mesh, material, (int)flags);
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<ActorComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<ActorComponentStorage>()->remove(entity);
}


///////////////////////// Scene::has_component //////////////////////////////
template<>
bool Scene::has_component<ActorComponent>(Scene::EntityId entity) const
{
  assert(get(entity) != nullptr);

  return system<ActorComponentStorage>()->has(entity);
}


///////////////////////// Scene::get_component //////////////////////////////
template<>
ActorComponent Scene::get_component<ActorComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  return system<ActorComponentStorage>()->get(entity);
}
