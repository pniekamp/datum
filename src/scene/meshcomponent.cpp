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

    auto &flags(size_t index) { return std::get<meshflags>(m_data)[index]; }
    auto &entity(size_t index) { return std::get<entityid>(m_data)[index]; }
    auto &bound(size_t index) { return std::get<boundingbox>(m_data)[index]; }
    auto &mesh(size_t index) { return std::get<meshresource>(m_data)[index]; }
    auto &material(size_t index) { return std::get<materialresource>(m_data)[index]; }

    size_t staticpartition() const { return m_staticpartition; }

    auto const &tree() const { return m_tree; }

  public:

    void clear();

    void add(EntityId entity, Bound3 const &bound, Mesh const *mesh, Material const *material, long flags);
    void remove(EntityId entity);

  private:

    size_t m_staticpartition;

    FreeList m_freelist;

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
    m_tree(StackAllocatorWithFreelist<>(allocator.arena(), m_freelist))
{
  m_staticpartition = 1;
}


///////////////////////// MeshStorage::clear ////////////////////////////////
void MeshStoragePrivate::clear()
{
  m_tree.clear();
  DefaultStorage::clear();

  m_staticpartition = 1;
}


///////////////////////// MeshStorage::add //////////////////////////////////
void MeshStoragePrivate::add(EntityId entity, Bound3 const &bound, Mesh const *mesh, Material const *material, long flags)
{
  DefaultStorage::add(entity);

  auto index = this->index(entity);

  get<meshflags>(m_data)[index] = flags;
  get<entityid>(m_data)[index] = entity;
  get<boundingbox>(m_data)[index] = bound;
  get<meshresource>(m_data)[index] = mesh;
  get<materialresource>(m_data)[index] = material;

  if (flags & MeshComponent::Static)
  {
    for_each(m_data, [=](auto &v) { swap(v[index], v[m_staticpartition]); });

    m_index[this->entity(index).index()] = index;
    m_index[this->entity(m_staticpartition).index()] = m_staticpartition;

    index = m_staticpartition;

    m_staticpartition += 1;

    m_tree.insert(MeshIndex{ index, this });
  }
}


///////////////////////// MeshStorage::remove ///////////////////////////////
void MeshStoragePrivate::remove(EntityId entity)
{
  auto index = this->index(entity);

  if (flags(index) & MeshComponent::Static)
  {
    m_tree.remove(MeshIndex{ index, this });
  }

  DefaultStorage::remove(entity);

  while (m_freeslots.size() != 0)
  {
    auto index = m_freeslots.front();

    for(auto i = index; i+1 < size(); ++i)
    {
      for_each(m_data, [=](auto &v) { v[i] = v[i+1]; });

      m_index[this->entity(i).index()] = i;
    }

    for_each(m_data, [](auto &v) { v.resize(v.size()-1); });

    if (index < m_staticpartition)
      m_staticpartition -= 1;

    m_freeslots.pop_front();
  }
}


///////////////////////// MeshStorage::tree /////////////////////////////////
MeshComponentStorage::iterator_pair<MeshComponentStorage::tree_iterator> MeshComponentStorage::tree() const
{
  auto storage = static_cast<MeshStoragePrivate const*>(this);

  return { storage->tree().begin(), storage->tree().end() };
}


///////////////////////// MeshStorage::dynamic //////////////////////////////
MeshComponentStorage::iterator_pair<Scene::EntityId const *> MeshComponentStorage::dynamic() const
{
  auto storage = static_cast<MeshStoragePrivate const*>(this);

  return { &data<entityid>(storage->staticpartition()), &data<entityid>(0) + size() };
}


///////////////////////// MeshStorage::update ///////////////////////////////
void MeshComponentStorage::update(TransformComponentStorage *transforms)
{
  assert(transforms);

  auto storage = static_cast<MeshStoragePrivate*>(this);

  for(size_t index = storage->staticpartition(); index < storage->size(); ++index)
  {
    assert(transforms->has(data<entityid>(index)));

    get<boundingbox>(m_data)[index] = transforms->world(data<entityid>(index)) * data<meshresource>(index)->bound;
  }
}


///////////////////////// Scene::initialise_storage /////////////////////////
template<>
void Scene::initialise_component_storage<MeshComponent>()
{
  m_systems[typeid(MeshComponentStorage)] = new(allocator<MeshStoragePrivate>().allocate(1)) MeshStoragePrivate(this, allocator());
}



//|---------------------- MeshComponent -------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Scene::add_component //////////////////////////////
template<>
MeshComponent Scene::add_component<MeshComponent>(Scene::EntityId entity, Mesh const *mesh, Material const *material, long flags)
{
  assert(get(entity) != nullptr);
  assert(system<TransformComponentStorage>());
  assert(system<TransformComponentStorage>()->has(entity));

  auto storage = static_cast<MeshStoragePrivate*>(system<MeshComponentStorage>());

  auto bound = system<TransformComponentStorage>()->world(entity) * mesh->bound;

  storage->add(entity, bound, mesh, material, flags);

  return { storage->index(entity), storage };
}


///////////////////////// Scene::remove_component ///////////////////////////
template<>
void Scene::remove_component<MeshComponent>(Scene::EntityId entity)
{
  assert(get(entity) != nullptr);

  auto storage = static_cast<MeshStoragePrivate*>(system<MeshComponentStorage>());

  storage->remove(entity);
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

  auto storage = static_cast<MeshStoragePrivate*>(system<MeshComponentStorage>());

  return { storage->index(entity), storage };
}


///////////////////////// MeshComponent::Constructor ////////////////////////
MeshComponent::MeshComponent(size_t index, MeshComponentStorage *storage)
  : index(index),
    storage(storage)
{
}
