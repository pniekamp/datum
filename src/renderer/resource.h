//
// Datum - resources
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "datum.h"
#include "datum/asset.h"
#include "vulkan.h"
#include <vector>
#include <bitset>

//|---------------------- ResourceManager -----------------------------------
//|--------------------------------------------------------------------------

class ResourceManager
{
  public:

    typedef StackAllocator<> allocator_type;

    ResourceManager(AssetManager &assets, allocator_type const &allocator);

    ResourceManager(ResourceManager const &) = delete;

  public:

    // initialise resource storage
    void initialise_slab(size_t slabsize);
    void initialise_device(VkPhysicalDevice physicaldevice, VkDevice device, VkQueue transferqueue, uint32_t transferqueuefamily, size_t buffersize, size_t maxbuffersize);

    // token
    size_t token();

    // allocate resource
    template<typename Resource, typename ...Args>
    Resource *allocate(Args... args);

    // create resource
    template<typename Resource, typename ...Args>
    Resource const *create(Args... args);

    // update resource
    template<typename Resource, typename ...Args>
    void update(Resource const *resource, Args... args);

    // request resource
    template<typename Resource>
    void request(DatumPlatform::PlatformInterface &platform, Resource const *resource);

    // release resource (deferred destroy)
    template<typename Resource>
    void release(Resource const *resource);

    // destroy resource
    template<typename Resource>
    void destroy(Resource const *resource);

    // release resources
    void release(size_t token);

  public:

    AssetManager *assets() { return m_assets; }

  private:

    allocator_type m_allocator;

  private:

    struct Slot
    {
      alignas(16) char data[256];
    };

    Slot *m_slots;

    void *acquire_slot(size_t size);
    void release_slot(void *slot, size_t size);

    size_t m_slathead;
    size_t m_slatsize;

    std::vector<std::bitset<64>, StackAllocator<std::bitset<64>>> m_slat;

#ifndef NDEBUG
    size_t m_slatused;
#endif

  public:

    struct TransferLump
    {
      Vulkan::Fence fence;
      Vulkan::CommandPool commandpool;
      Vulkan::CommandBuffer commandbuffer;
      Vulkan::StorageBuffer transferbuffer;

      template<typename View = void>
      View *memory(VkDeviceSize offset = 0) const
      {
        return (View*)((uint8_t*)transfermemory + offset);
      }

      void *transfermemory;
    };

    TransferLump const *acquire_lump(size_t size);

    void release_lump(TransferLump const *lump);

  private:

    Vulkan::VulkanDevice vulkan;

    struct Buffer
    {
      VkDeviceSize offset, used, size;

      TransferLump transferlump;

      Buffer *next;

      uint8_t data[1];
    };

    Buffer *m_buffers;

    size_t m_buffersallocated, m_minallocation, m_maxallocation;

#ifndef NDEBUG
    size_t m_bufferused;
#endif

    void submit(TransferLump const *lump);
    void submit(VkCommandBuffer setupbuffer, VkFence fence);

  private:

    struct deleterbase
    {
      virtual void destroy(ResourceManager *manager, void const *slot) = 0;
    };

    template<typename Resource>
    class deleter : public deleterbase
    {
      virtual void destroy(ResourceManager *manager, void const *resource) override
      {
        manager->destroy<Resource>(reinterpret_cast<Resource const *>(resource));
      }
    };

    struct deleterholder
    {
      deleterholder() = default;

      template<typename Resource>
      deleterholder(Resource const *resource)
        : resource(resource)
      {
        new(storage) deleter<Resource>;
      }

      deleterbase *operator ->() { return (deleterbase*)storage; }

      void const *resource;
      alignas(std::max_align_t) char storage[sizeof(deleterbase)];
    };

    size_t m_deletershead;
    size_t m_deleterstail;
    std::vector<deleterholder, StackAllocator<deleterholder>> m_deleters;

    template<typename Resource>
    void defer_destroy(Resource const *resource)
    {
      leap::threadlib::SyncLock lock(m_mutex);

      m_deleters[m_deleterstail++ % m_deleters.size()] = resource;
    }

  private:

    AssetManager *m_assets;

    mutable leap::threadlib::SpinLock m_mutex;
};

// unique_resource

template<typename Resource>
class unique_resource
{
  public:
    unique_resource()
      : m_resource(nullptr), m_resources(nullptr)
    {
    }

    unique_resource(ResourceManager *resources, Resource const *resource)
      : m_resource(resource), m_resources(resources)
    {
    }

    unique_resource(ResourceManager &resources, Resource const *resource)
      : m_resource(resource), m_resources(&resources)
    {
    }

    unique_resource(unique_resource const &) = delete;

    unique_resource(unique_resource &&other) noexcept
      : unique_resource()
    {
      *this = std::move(other);
    }

    ~unique_resource()
    {
      if (m_resource)
        m_resources->release(m_resource);
    }

    unique_resource &operator=(unique_resource &&other) noexcept
    {
      std::swap(m_resources, other.m_resources);
      std::swap(m_resource, other.m_resource);

      return *this;
    }

    Resource const *release() { auto resource = m_resource; m_resource = 0; return resource; }

    operator Resource const *() const { return m_resource; }

    Resource const *operator *() const { return m_resource; }
    Resource const *operator ->() const { return m_resource; }

  private:

    Resource const *m_resource;
    ResourceManager *m_resources;
};

// Request Utility
template<typename Resource>
void request(DatumPlatform::PlatformInterface &platform, ResourceManager &resources, Resource const *resource, int *ready, int *total)
{
  *total += 1;

  if (resource)
  {
    resources.request(platform, resource);

    if (resource->ready())
    {
      *ready += 1;
    }
  }
}

// Initialise
bool initialise_resource_system(DatumPlatform::PlatformInterface &platform, ResourceManager &resourcemanager, size_t slabsize, size_t buffersize, size_t maxbuffersize, uint32_t queueindex);
