//
// Datum - component storage
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "scene.h"
#include <vector>
#include <tuple>
#include <utility>

//|---------------------- Storage -------------------------------------------
//|--------------------------------------------------------------------------

class Storage
{
  protected:
    Storage(Scene *scene);
    virtual ~Storage();

    virtual void clear() = 0;

    virtual void destroyed(Scene::EntityId entity) = 0;

  protected:

    Scene *m_scene;

    friend class Scene;
};

///////////////////////// Storage::Constructor //////////////////////////////
inline Storage::Storage(Scene *scene)
{
  m_scene = scene;
}


///////////////////////// Storage::Destructor ///////////////////////////////
inline Storage::~Storage()
{
}


//|---------------------- DefaultStorage ------------------------------------
//|--------------------------------------------------------------------------

template<typename ...Types>
class DefaultStorage : public Storage
{
  protected:

    typedef StackAllocator<> allocator_type;

    DefaultStorage(Scene *scene, allocator_type const &allocator);

    DefaultStorage(DefaultStorage const &) = delete;

    using EntityId = Scene::EntityId;

  public:

    bool has(EntityId entity) const;

  public:

    class iterator
    {
      public:
        explicit iterator(size_t index, DefaultStorage const *storage)
          : index(index), storage(storage)
        {
          if (index != storage->size() && storage->data<0>(index) == 0)
            ++*this;
        }

        bool operator ==(iterator const &that) const { return index == that.index; }
        bool operator !=(iterator const &that) const { return index != that.index; }

        EntityId const &operator *() const { return storage->data<0>(index); }
        EntityId const *operator ->() const { return &storage->data<0>(index); }

        iterator &operator++()
        {
          ++index;

          while (index != storage->size() && storage->data<0>(index) == 0)
            ++index;

          return *this;
        }

      private:

        size_t index;
        DefaultStorage const *storage;
    };

    template<typename Iterator>
    class iterator_pair : public std::pair<Iterator, Iterator>
    {
      public:
        using std::pair<Iterator, Iterator>::pair;

        Iterator begin() const { return this->first; }
        Iterator end() const { return this->second; }
    };

    iterator_pair<iterator> entities() const
    {
      return { iterator{ 0, this }, iterator{ this->size(), this } };
    }

  protected:

    void clear() override;

    size_t index(EntityId entity) const;

    template<size_t typeindex>
    auto &data(size_t index)
    {
      return std::get<typeindex>(m_data)[index];
    }

    template<size_t typeindex>
    auto const &data(size_t index) const
    {
      return std::get<typeindex>(m_data)[index];
    }

  protected:

    size_t size() const { return std::get<0>(m_data).size(); }

    size_t add(EntityId entity);
    virtual void remove(EntityId entity);

  protected:

    virtual void destroyed(Scene::EntityId entity) override;

  protected:

    template<typename Tuple, typename Fn, size_t... Indices>
    void for_each(Tuple &&tuple, Fn &&f, std::index_sequence<Indices...>)
    {
      using sink = int[];
      (void)(sink{ (std::forward<Fn>(f)(std::get<Indices>(std::forward<Tuple>(tuple))), 0)... });
    }

    template<typename Tuple, typename Fn>
    void for_each(Tuple &&tuple, Fn &&f)
    {
      for_each(std::forward<Tuple>(tuple), std::forward<Fn>(f), std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>());
    }

  protected:

    std::vector<size_t, StackAllocator<size_t>> m_index;

    std::tuple<std::vector<Types, StackAllocator<Types>>...> m_data;

    std::deque<size_t, StackAllocator<size_t>> m_freeslots;

    friend class Scene;
};


//|---------------------- DefaultStorage ------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// DefaultStorage::Constructor ///////////////////////
template<typename ...Types>
DefaultStorage<Types...>::DefaultStorage(Scene *scene, allocator_type const &allocator)
  : Storage(scene),
    m_index(allocator),
    m_data(std::allocator_arg, allocator),
    m_freeslots(allocator)
{
  clear();
}


///////////////////////// DefaultStorage::clear /////////////////////////////
template<typename ...Types>
void DefaultStorage<Types...>::clear()
{
  m_index.clear();

  for_each(m_data, [](auto &v) { v.resize(1); });

  m_freeslots.clear();
}


///////////////////////// DefaultStorage::has ///////////////////////////////
template<typename ...Types>
bool DefaultStorage<Types...>::has(EntityId entity) const
{
  return (entity.index() < m_index.size()) && (m_index[entity.index()] != 0);
}


///////////////////////// DefaultStorage::index /////////////////////////////
template<typename ...Types>
size_t DefaultStorage<Types...>::index(EntityId entity) const
{
  assert(has(entity));

  return m_index[entity.index()];
}


///////////////////////// DefaultStorage::add ///////////////////////////////
template<typename ...Types>
size_t DefaultStorage<Types...>::add(EntityId entity)
{
  assert(!has(entity));

  m_index.resize(std::max(m_index.size(), entity.index()+1));

  size_t index = 0;

  if (m_freeslots.size() != 0)
  {
    index = m_freeslots.front();

    m_freeslots.pop_front();
  }
  else
  {
    for_each(m_data, [](auto &v) { v.resize(v.size()+1); });

    index = size() - 1;
  }

  m_index[entity.index()] = index;

  return index;
}


///////////////////////// DefaultStorage::remove ////////////////////////////
template<typename ...Types>
void DefaultStorage<Types...>::remove(EntityId entity)
{
  assert(has(entity));

  auto index = m_index[entity.index()];

  for_each(m_data, [=](auto &v) { v[index] = {}; });

  m_freeslots.push_back(index);

  m_index[entity.index()] = 0;
}


///////////////////////// DefaultStorage::destroyed /////////////////////////
template<typename ...Types>
void DefaultStorage<Types...>::destroyed(Scene::EntityId entity)
{
  if (has(entity))
  {
    remove(entity);
  }
}
