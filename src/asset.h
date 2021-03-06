//
// Datum - asset system
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "datum.h"
#include "datum/memory.h"
#include <leap/threadcontrol.h>
#include <vector>

//|---------------------- Asset ---------------------------------------------
//|--------------------------------------------------------------------------

struct Asset
{
  size_t id;

  union
  {
    struct // catalog info
    {
      uint32_t magic;
      uint32_t version;
    };

    struct // text info
    {
      int length;
    };

    struct // image info
    {
      int width;
      int height;
      int layers;
      int levels;
      int format;
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
      int bonecount;
      float mincorner[3];
      float maxcorner[3];
    };

    struct // material info
    {
    };

    struct // animation info
    {
      float duration;
      int jointcount;
      int transformcount;
    };

    struct // particle system
    {
      float minrange[3];
      float maxrange[3];
      uint32_t maxparticles;
      uint32_t emittercount;
    };

    struct // model info
    {
      int texturecount;
      int materialcount;
      int meshcount;
      int instancecount;
    };
  };

  size_t datasize;
};


//|---------------------- AssetManager --------------------------------------
//|--------------------------------------------------------------------------

class AssetManager
{
  public:

    using allocator_type = StackAllocator<>;

    AssetManager(allocator_type const &allocator);

    AssetManager(AssetManager const &) = delete;

  public:

    // initialise asset storage
    void initialise(size_t maxcount, size_t slabsize);

    // load asset pack
    Asset const *load(DatumPlatform::PlatformInterface &platform, const char *identifier);

    // find
    Asset const *find(size_t id) const;

    // Request asset payload. May not be loaded, will initiate background load and return null.
    void const *request(DatumPlatform::PlatformInterface &platform, Asset const *asset);

  public:

    uintptr_t acquire_barrier();

    void release_barrier(uintptr_t barrier);

#ifndef NDEBUG
  std::atomic<size_t> barriercount;
#endif

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

      size_t size;

      Slot *after;

      Slot *prev;
      Slot *next;

      alignas(16) uint8_t data[1];
    };

    Slot *m_head;

    Slot *acquire_slot(size_t size);

    Slot *touch_slot(Slot *slot);

    static void background_loader(DatumPlatform::PlatformInterface &platform, void *ldata, void *rdata);

  private:

    mutable leap::threadlib::SpinLock m_mutex;
};

// protect against eviction
struct asset_guard
{
  asset_guard(AssetManager &assets)
    : m_assets(&assets)
  {
    m_barrier = m_assets->acquire_barrier();
  }

  asset_guard(AssetManager *assets)
    : asset_guard(*assets)
  {
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
bool initialise_asset_system(DatumPlatform::PlatformInterface &platform, AssetManager &assetmanager, size_t slotcount, size_t slabsize);
