//
// Arena Memory
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "platform.h"
#include <memory>
#include <algorithm>
#include <scoped_allocator>
#include <cassert>

// Arena

using Arena = DatumPlatform::GameMemory;

//|---------------------- StackAllocator ------------------------------------
//|--------------------------------------------------------------------------

template<typename T = void*>
class StackAllocator
{
  public:

    typedef T value_type;

    template<typename U>
    struct rebind
    {
      typedef StackAllocator<U> other;
    };

  public:

    StackAllocator(Arena &arena);

    template<typename U>
    StackAllocator(StackAllocator<U> const &other);

    Arena &arena() const { return *m_arena; }

    T *allocate(std::size_t n, std::size_t alignment = alignof(T));

    void deallocate(T * const ptr, std::size_t n);

  private:

    Arena *m_arena;
};


///////////////////////// StackAllocator::Constructor ///////////////////////
template<typename T>
StackAllocator<T>::StackAllocator(Arena &arena)
  : m_arena(&arena)
{
}


///////////////////////// StackAllocator::rebind ////////////////////////////
template<typename T>
template<typename U>
StackAllocator<T>::StackAllocator(StackAllocator<U> const &other)
  : StackAllocator(other.arena())
{  
}


///////////////////////// StackAllocator::allocate //////////////////////////
template<typename T>
T *StackAllocator<T>::allocate(std::size_t n, std::size_t alignment)
{
  std::size_t size = n * sizeof(T);

  void *result = static_cast<char*>(m_arena->data) + m_arena->size;

  std::size_t space = m_arena->capacity - m_arena->size;

  if (!std::align(alignment, size, result, space))
    throw std::bad_alloc();

  m_arena->size = static_cast<char*>(result) + size - static_cast<char*>(m_arena->data);

  return static_cast<T*>(result);
}


///////////////////////// StackAllocator::deallocate ////////////////////////
template<typename T>
void StackAllocator<T>::deallocate(T * const ptr, std::size_t n)
{
}


///////////////////////// StackAllocator::operator == ///////////////////////
template<typename T, typename U>
bool operator ==(StackAllocator<T> const &lhs, StackAllocator<U> const &rhs)
{
  return lhs.arena().data == rhs.arena().data;
}


///////////////////////// StackAllocator::operator != ///////////////////////
template<typename T, typename U>
bool operator !=(StackAllocator<T> const &lhs, StackAllocator<U> const &rhs)
{
  return !(lhs == rhs);
}



//|---------------------- Freelist ------------------------------------------
//|--------------------------------------------------------------------------

class FreeList
{
  public:

    FreeList() = default;

    void *acquire(std::size_t bytes, std::size_t alignment);

    void release(void * const ptr, std::size_t bytes);

  private:

    template<typename U, std::size_t ulignment = alignof(U)>
    U *aligned(void *ptr)
    {
      return reinterpret_cast<U*>((reinterpret_cast<std::size_t>(ptr) + ulignment - 1) & -ulignment);
    }

    size_t bucket(size_t n)
    {      
#if defined(_MSC_VER) && defined(_WIN64)
      auto __builtin_clzll = [](unsigned long long mask)
      {
        unsigned long where = 0;

        _BitScanReverse64(&where, mask);

        return 63 - where;
      };
#endif
      return std::min(std::max(8*sizeof(unsigned long long) - __builtin_clzll(n*n-n), (size_t)10) - 10, std::extent<decltype(m_freelist)>::value);
    }

    struct Node
    {
      std::size_t bytes;

      void *next;
    };

    void *m_freelist[24] = {};

    friend void dump(const char *name, FreeList const &freelist);
};


///////////////////////// Freelist::acquire /////////////////////////////////
inline void *FreeList::acquire(std::size_t bytes, std::size_t alignment)
{
  auto index = bucket(bytes);

  void *entry = m_freelist[index];
  void **into = &m_freelist[index];

  while (entry != nullptr)
  {
    Node *node = aligned<Node>(entry);

    if (node->bytes == bytes && ((size_t)entry & (alignment-1)) == 0)
    {
      *into = node->next;

      return entry;
    }

    into = &node->next;
    entry = node->next;
  }

  return nullptr;
}


///////////////////////// Freelist::release /////////////////////////////////
inline void FreeList::release(void * const ptr, std::size_t bytes)
{
  Node *node = aligned<Node>(ptr);

  if ((size_t)node + sizeof(Node) < (size_t)ptr + bytes)
  {
    auto index = bucket(bytes);

    node->bytes = bytes;
    node->next = m_freelist[index];

    m_freelist[index] = ptr;
  }
}


//|---------------------- StackAllocatorWithFreelist ------------------------
//|--------------------------------------------------------------------------

template<typename T = void*>
class StackAllocatorWithFreelist : public StackAllocator<T>
{
  public:

    typedef T value_type;

    template<typename U>
    struct rebind
    {
      typedef StackAllocatorWithFreelist<U> other;
    };

  public:

    StackAllocatorWithFreelist(Arena &arena, FreeList &freelist);

    template<typename U>
    StackAllocatorWithFreelist(StackAllocator<U> const &other, FreeList &freelist);

    template<typename U>
    StackAllocatorWithFreelist(StackAllocatorWithFreelist<U> const &other);

    FreeList &freelist() const { return *m_freelist; }

    T *allocate(std::size_t n, std::size_t alignment = alignof(T));

    void deallocate(T * const ptr, std::size_t n);

  private:

    FreeList *m_freelist;
};


///////////////////////// StackAllocatorWithFreelist::Constructor ///////////
template<typename T>
StackAllocatorWithFreelist<T>::StackAllocatorWithFreelist(Arena &arena, FreeList &freelist)
  : StackAllocator<T>(arena)
{
  m_freelist = &freelist;
}


///////////////////////// StackAllocatorWithFreelist::Constructor ///////////
template<typename T>
template<typename U>
StackAllocatorWithFreelist<T>::StackAllocatorWithFreelist(StackAllocator<U> const &other, FreeList &freelist)
  : StackAllocatorWithFreelist(other.arena(), freelist)
{
}


///////////////////////// StackAllocatorWithFreelist::rebind ////////////////
template<typename T>
template<typename U>
StackAllocatorWithFreelist<T>::StackAllocatorWithFreelist(StackAllocatorWithFreelist<U> const &other)
  : StackAllocatorWithFreelist(other.arena(), other.freelist())
{
}


///////////////////////// StackAllocatorWithFreelist::allocate //////////////
template<typename T>
T *StackAllocatorWithFreelist<T>::allocate(std::size_t n, std::size_t alignment)
{
  auto result = m_freelist->acquire(n*sizeof(T), alignment);

  if (!result)
  {
    result = StackAllocator<T>::allocate(n, alignment);
  }

  return static_cast<T*>(result);
}


///////////////////////// StackAllocatorWithFreelist::deallocate ////////////
template<typename T>
void StackAllocatorWithFreelist<T>::deallocate(T * const ptr, std::size_t n)
{
  m_freelist->release(ptr, n*sizeof(T));
}



//|---------------------- misc routines -------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// inarena ///////////////////////////////////////////
template<typename T>
bool inarena(Arena &arena, T *ptr)
{
  return arena.data <= ptr && ptr < (void*)((char*)arena.data + arena.size);
}


///////////////////////// allocate //////////////////////////////////////////
template<typename T, std::size_t alignment = alignof(T)>
T *allocate(StackAllocator<> const &allocator, std::size_t n = 1)
{
  return typename StackAllocator<>::template rebind<T>::other(allocator).allocate(n, alignment);
}


///////////////////////// allocate //////////////////////////////////////////
inline Arena allocate(StackAllocator<> const &allocator, std::size_t slabsize)
{
  Arena result;

  result.size = 0;
  result.capacity = slabsize;
  result.data = allocate<char>(allocator, slabsize);

  return result;
}


///////////////////////// mark //////////////////////////////////////////////
inline size_t mark(Arena &arena)
{
  return arena.size;
}


///////////////////////// rewind ////////////////////////////////////////////
inline void rewind(Arena &arena, size_t mark)
{
  arena.size = mark;
}

