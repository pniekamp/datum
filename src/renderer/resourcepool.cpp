//
// Datum - resource pool
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "resourcepool.h"
#include "renderer.h"
#include <leap.h>
#include <numeric>
#include "debug.h"

using namespace std;
using namespace Vulkan;
using leap::alignto;
using leap::extentof;

namespace
{
  size_t tid()
  {
    static std::atomic<size_t> threadcount(0);
    static thread_local size_t threadid = threadcount++;

    return threadid;
  }

} // namespace


///////////////////////// ResourcePool::initialise //////////////////////////
void ResourcePool::initialise(VkPhysicalDevice physicaldevice, VkDevice device, VkQueue renderqueue, uint32_t renderqueuefamily, size_t storagesize)
{
  initialise_vulkan_device(&vulkan, physicaldevice, device, renderqueue, renderqueuefamily);

  VkDeviceSize alignment = vulkan.physicaldeviceproperties.limits.minStorageBufferOffsetAlignment;

  VkDeviceSize slotsize = alignto(storagesize / StorageBufferSlots - alignment + 1, alignment);

  assert(slotsize >= alignment);
  assert(alignment >= alignof(max_align_t));
  assert(slotsize <= vulkan.physicaldeviceproperties.limits.maxStorageBufferRange);

  m_storagehead = 0;

  m_transferbuffer = create_transferbuffer(vulkan, slotsize * StorageBufferSlots + slotsize);

  m_transfermemory = map_memory<uint8_t>(vulkan, m_transferbuffer, 0, m_transferbuffer.size);

  for(size_t i = 0; i < StorageBufferSlots; ++i)
  {
    m_storagebuffers[i].base = i * slotsize;
    m_storagebuffers[i].size = slotsize;
    m_storagebuffers[i].used = 0;
    m_storagebuffers[i].buffer = m_transferbuffer;

    m_storagebuffers[i].refcount = 0;
  }

  VkDescriptorPoolSize typecounts[2] = {};
  typecounts[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
  typecounts[0].descriptorCount = DescriptorSetSlots;
  typecounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  typecounts[1].descriptorCount = 8*DescriptorSetSlots;

  VkDescriptorPoolCreateInfo descriptorpoolinfo = {};
  descriptorpoolinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorpoolinfo.maxSets = accumulate(begin(typecounts), end(typecounts), 0, [](int i, auto &k) { return i + k.descriptorCount; });
  descriptorpoolinfo.poolSizeCount = extentof(typecounts);
  descriptorpoolinfo.pPoolSizes = typecounts;

  for(size_t i = 0; i < ResourceLumpCount; ++i)
  {
    m_lumps[i].storagepool.count = 0;

    m_lumps[i].commandpool = create_commandpool(vulkan, 0);

    m_lumps[i].descriptorpool = create_descriptorpool(vulkan, descriptorpoolinfo);
  }

  RESOURCE_USE(RenderLump, (m_lumpsused = 0), ResourceLumpCount)
  RESOURCE_USE(RenderStorage, (m_storageused = 0), m_transferbuffer.size)

  m_initialised = true;
}


///////////////////////// ResourcePool::reset_storagepool ///////////////////
void ResourcePool::reset_storagepool(StoragePool &pool)
{
  for(size_t i = 0; i < pool.count; ++i)
  {  
    m_storagebuffers[pool.buffers[i] - m_storagebuffers].refcount -= 1;
  }

  pool.count = 0;
}


///////////////////////// ResourcePool::acquire_lump ////////////////////////
ResourcePool::ResourceLump const *ResourcePool::acquire_lump()
{
  if (!m_initialised)
    return nullptr;

  static thread_local int lumphead = (tid() % 8) * 8;

  for(size_t i = 0; i < ResourceLumpCount; ++i)
  {
    ResourceLump &lump = m_lumps[(lumphead + i) % ResourceLumpCount];

    if (lump.lock.test_and_set(std::memory_order_acquire) == false)
    {
      RESOURCE_USE(RenderLump, (m_lumpsused += 1), ResourceLumpCount)

      return &lump;
    }
  }

  LOG_ONCE("Lump Pool Exhausted");

  return nullptr;
}


///////////////////////// ResourcePool::release_lump ////////////////////////
void ResourcePool::release_lump(ResourceLump const *lumphandle)
{
  assert(m_initialised);
  assert(lumphandle >= m_lumps && lumphandle - m_lumps < ResourceLumpCount);

  ResourceLump &lump = m_lumps[lumphandle - m_lumps];

  RESOURCE_USE(RenderLump, (m_lumpsused -= 1), ResourceLumpCount)

  reset_storagepool(lump.storagepool);
  reset_commandpool(vulkan, lump.commandpool);
  reset_descriptorpool(vulkan, lump.descriptorpool);

  lump.lock.clear(std::memory_order_release);
}


///////////////////////// ResourcePool::acquire_commandbuffer ///////////////
ResourcePool::CommandBuffer ResourcePool::acquire_commandbuffer(ResourceLump const *lumphandle)
{
  assert(m_initialised);
  assert(lumphandle >= m_lumps && lumphandle - m_lumps < ResourceLumpCount);

  ResourceLump &lump = m_lumps[lumphandle - m_lumps];

  auto commandbuffer = allocate_commandbuffer(vulkan, lump.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

  return { commandbuffer.release() };
}


///////////////////////// ResourcePool::acquire_storage /////////////////////
ResourcePool::StorageBuffer ResourcePool::acquire_storagebuffer(ResourceLump const *lumphandle, size_t required)
{
  assert(m_initialised);
  assert(lumphandle >= m_lumps && lumphandle - m_lumps < ResourceLumpCount);
  assert(m_lumps[lumphandle - m_lumps].storagepool.count < extent<decltype(StoragePool::buffers)>::value);

  assert(required < m_transferbuffer.size / StorageBufferSlots);

  ResourceLump &lump = m_lumps[lumphandle - m_lumps];

  size_t storagehead = m_storagehead;

  for(size_t i = 0; i < StorageBufferSlots; ++i)
  {
    StorageSlot &buffer = m_storagebuffers[(storagehead + i) % StorageBufferSlots];

    if (buffer.lock.test_and_set(std::memory_order_acquire) == false)
    {
      if (buffer.refcount == 0)
      {
        RESOURCE_USE(RenderStorage, (m_storageused -= buffer.used), m_transferbuffer.size)

        buffer.used = 0;
      }

      if (buffer.used + required < buffer.size && lump.storagepool.count < extentof(lump.storagepool.buffers))
      {
        lump.storagepool.buffers[lump.storagepool.count++] = &buffer;

        buffer.refcount += 1;

        RESOURCE_USE(RenderStorage, (m_storageused += buffer.size - buffer.used), m_transferbuffer.size)

        m_storagehead = i;

        void *memory = m_transfermemory + buffer.base + buffer.used;
        VkDeviceSize alignment = vulkan.physicaldeviceproperties.limits.minStorageBufferOffsetAlignment;

        return { memory, alignment, buffer.buffer, buffer.base + buffer.used, buffer.size - buffer.used, &buffer };
      }

      buffer.lock.clear(std::memory_order_release);
    }
  }

  LOG_ONCE("Storage Buffers Exhausted");

  return {};
}


///////////////////////// ResourcePool::release_storage ///////////////////////////////
void ResourcePool::release_storagebuffer(StorageBuffer const &storage, size_t used)
{
  assert(m_initialised);
  assert(storage.storagebuffer >= m_storagebuffers && storage.storagebuffer - m_storagebuffers < StorageBufferSlots);

  StorageSlot &buffer = m_storagebuffers[storage.storagebuffer - m_storagebuffers];

  RESOURCE_USE(RenderStorage, (m_storageused -= buffer.size - buffer.used - used), m_transferbuffer.size)

  VkDeviceSize alignment = vulkan.physicaldeviceproperties.limits.minStorageBufferOffsetAlignment;

  buffer.used = alignto(buffer.used + used, alignment);

  buffer.lock.clear(std::memory_order_release);
}


///////////////////////// ResourcePool::acquire_descriptorset ///////////////
ResourcePool::DescriptorSet ResourcePool::acquire_descriptorset(ResourceLump const *lumphandle, VkDescriptorSetLayout layout, StorageBuffer const &buffer)
{
  assert(m_initialised);
  assert(lumphandle >= m_lumps && lumphandle - m_lumps < ResourceLumpCount);
  assert(m_lumps[lumphandle - m_lumps].storagepool.count < extent<decltype(StoragePool::buffers)>::value);

  ResourceLump &lump = m_lumps[lumphandle - m_lumps];

  auto descriptorset = allocate_descriptorset(vulkan, lump.descriptorpool, layout);

  bind_buffer(vulkan, descriptorset, 0, buffer.buffer, buffer.offset, buffer.capacity, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);

  return { descriptorset.release() };
}


///////////////////////// initialise_resource_pool //////////////////////////
bool initialise_resource_pool(DatumPlatform::PlatformInterface &platform, ResourcePool &resourcepool, size_t storagesize, uint32_t queueindex)
{
  auto renderdevice = platform.render_device();

  resourcepool.initialise(renderdevice.physicaldevice, renderdevice.device, renderdevice.queues[queueindex].queue, renderdevice.queues[queueindex].familyindex, storagesize);

  return true;
}
