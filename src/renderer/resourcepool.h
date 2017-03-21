//
// Datum - resource pool
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "datum.h"
#include "vulkan.h"
#include <atomic>

//|---------------------- ResourcePool --------------------------------------
//|--------------------------------------------------------------------------

class ResourcePool
{
  private:

    struct StorageSlot
    {
      VkDeviceSize base;
      VkDeviceSize size;
      VkDeviceSize used;
      VkBuffer buffer;

      std::atomic<size_t> refcount;

      std::atomic_flag lock = ATOMIC_FLAG_INIT;
    };

    struct StoragePool
    {
      size_t count;
      StorageSlot const *buffers[128];
    };

    void reset_storagepool(StoragePool &pool);

  public:

    struct StorageBuffer
    {
      void *memory;
      VkDeviceSize alignment;

      VkBuffer buffer;
      VkDeviceSize offset;
      VkDeviceSize capacity;

      StorageSlot const *storagebuffer;
    };

    struct CommandBuffer
    {
      VkCommandBuffer commandbuffer;
    };

    struct DescriptorSet
    {
      VkDescriptorSet descriptorset;
    };

    struct ResourceLump
    {
      StoragePool storagepool;
      Vulkan::CommandPool commandpool;
      Vulkan::DescriptorPool descriptorpool;

      std::atomic_flag lock = ATOMIC_FLAG_INIT;
    };

    static constexpr int StorageBufferSlots = 256;
    static constexpr int CommandBufferSlots = 128;
    static constexpr int DescriptorSetSlots = 512;
    static constexpr int ResourceLumpCount = 64;

  public:

    // initialise resource pool

    void initialise(VkPhysicalDevice physicaldevice, VkDevice device, VkQueue renderqueue, uint32_t renderqueuefamily, size_t storagesize);

    // lump

    ResourceLump const *acquire_lump();

    void release_lump(ResourceLump const *lump);

    // command buffers

    CommandBuffer acquire_commandbuffer(ResourceLump const *lump);

    // storage buffers

    StorageBuffer acquire_storagebuffer(ResourceLump const *lump, size_t required);

    void release_storagebuffer(StorageBuffer const &storage, size_t used);

    // descriptor sets

    DescriptorSet acquire_descriptorset(ResourceLump const *lumpref, VkDescriptorSetLayout layout, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size);

  private:

    Vulkan::VulkanDevice vulkan;

    std::atomic<size_t> m_storagehead;

    StorageSlot m_storagebuffers[StorageBufferSlots];

    ResourceLump m_lumps[ResourceLumpCount];

#ifndef NDEBUG
    std::atomic<size_t> m_lumpsused;
    std::atomic<size_t> m_storageused;
#endif

  private:

    Vulkan::StorageBuffer m_transferbuffer;

    Vulkan::MemoryView<uint8_t> m_transfermemory;

    bool m_initialised = false;
};

// Initialise
bool initialise_resource_pool(DatumPlatform::PlatformInterface &platform, ResourcePool &resourcepool, size_t storagesize, uint32_t queueindex);
