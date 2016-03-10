//
// Datum - asset system
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "platform.h"
#include "memory.h"
#include "leap/threadcontrol.h"
#include <vector>


//|---------------------- Asset ---------------------------------------------
//|--------------------------------------------------------------------------

struct Asset
{
  size_t id;

  union
  {
    struct // text info
    {
      int length;
    };

    struct // image info
    {
      int width;
      int height;
      int layers;
      float alignx;
      float aligny;
    };

    struct // font info
    {
      int ascent;
      int descent;
      int leading;
      int glyphcount;
    };

    struct // mesh info
    {
      int vertexcount;
      int indexcount;
      float mincorner[3];
      float maxcorner[3];
    };

    struct // material info
    {
    };

    struct // model info
    {
      int texturecount;
      int materialcount;
      int meshcount;
      int instancecount;
    };

  };
};


//|---------------------- AssetManager --------------------------------------
//|--------------------------------------------------------------------------

class AssetManager
{
  public:

    typedef StackAllocator<> allocator_type;

    AssetManager(allocator_type const &allocator);

    AssetManager(AssetManager const &) = delete;

  public:

    // initialise asset storage
    void initialise(std::size_t maxcount, std::size_t slabsize);

    // load asset pack
    Asset const *load(DatumPlatform::PlatformInterface &platform, const char *identifier);

    // find
    Asset const *find(size_t id) const;

    // Request asset payload. May not be loaded, will initiate background load and return null.
    void const *request(DatumPlatform::PlatformInterface &platform, Asset const *asset);

  public:

    uintptr_t aquire_barrier();

    void release_barrier(uintptr_t barrier);

  private:

    allocator_type m_allocator;

  private:

    struct Slot;

    struct File
    {
      size_t baseid;

      DatumPlatform::PlatformInterface::handle_t handle;
    };

    std::vector<File, StackAllocator<File>> m_files;

    struct AssetEx : public Asset
    {
      File *file;

      uint64_t datapos;
      std::size_t datasize;

      Slot *slot;
    };

    std::vector<AssetEx, StackAllocator<AssetEx>> m_assets;

    struct Slot
    {
      enum class State
      {
        Empty,
        Barrier,
        Loading,
        Loaded
      };

      State state;

      AssetEx *asset;

      std::size_t size;

      Slot *after;

      Slot *prev;
      Slot *next;

      alignas(16) uint8_t data[];
    };

    Slot *m_head;

    Slot *aquire_slot(std::size_t size);

    Slot *touch_slot(Slot *slot);

    static void background_loader(DatumPlatform::PlatformInterface &platform, void *ldata, void *rdata);

  private:

    mutable leap::threadlib::SpinLock m_mutex;
};

// protect against eviction
struct asset_guard
{
  asset_guard(AssetManager *assets)
    : m_assets(assets)
  {
    m_barrier = m_assets->aquire_barrier();
  }

  ~asset_guard()
  {
    m_assets->release_barrier(m_barrier);
  }

  private:

    uintptr_t m_barrier;
    AssetManager *m_assets;
};

// Initialise
bool initialise_asset_system(DatumPlatform::PlatformInterface &platform, AssetManager &assetmanager);
