//
// Datum - resources
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "resource.h"
#include "assetpack.h"
#include "vulkan.h"
#include <leap.h>
#include <numeric>
#include <memory>
#include "debug.h"

using namespace std;
using leap::alignto;
using leap::extentof;

static constexpr size_t BufferAlignment = 4096;

//|---------------------- ResourceManager -----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::Constructor //////////////////////
ResourceManager::ResourceManager(AssetManager *assets, allocator_type const &allocator)
  : m_allocator(allocator),
    m_slat(allocator),
    m_deleters(allocator),
    m_assets(assets)
{
  m_slots = nullptr;
  m_slathead = 0;
  m_slatsize = 0;
  m_deletershead = 0;
  m_deleterstail = 0;

  m_buffers = nullptr;
}


///////////////////////// ResourceManager::initialise ///////////////////////
void ResourceManager::initialise_slab(size_t slabsize)
{
  int nslots = slabsize / sizeof(Slot);

  m_slots = StackAllocator<Slot>(m_allocator).allocate(nslots);

  m_slat.resize((nslots - 1) / 64 + 1);
  m_slatsize = nslots;

  RESOURCE_USE(ResourceSlot, (m_slatused = 0), m_slatsize)

  m_deletershead = 0;
  m_deleterstail = 0;
  m_deleters.resize(nslots);

  cout << "Resource Storage: " << slabsize / 1024 / 1024 << " MiB" << endl;
}


///////////////////////// ResourceManager::acquire_slot /////////////////////
void *ResourceManager::acquire_slot(size_t size)
{
  assert(size != 0);
  assert(size < m_slatsize * sizeof(Slot));

  leap::threadlib::SyncLock lock(m_mutex);

  size_t nslots = (size - 1) / sizeof(Slot) + 1;

  for(size_t k = 0; k < 2; ++k)
  {
    size_t j = m_slathead;

    while (j < m_slatsize - nslots)
    {
      size_t i = j;

      bool available;
      for(available = true; available && j < i+nslots; ++j)
        available &= (m_slat[j >> 6][j & 0x3F] == 0);

      if (available)
      {
        for(j = i; j < i+nslots; ++j)
          m_slat[j >> 6][j & 0x3F] = 1;

        RESOURCE_USE(ResourceSlot, (m_slatused += nslots), m_slatsize)

        m_slathead = j;

        return m_slots + i;
      }

      while (m_slat[j >> 6].all() && j < m_slatsize)
        j += 64;
    }

    m_slathead = 0;
  }

  LOG_ONCE("Resource Slots Exhausted");

  return nullptr;
}


///////////////////////// ResourceManager::release_slot /////////////////////
void ResourceManager::release_slot(void *slot, size_t size)
{
  leap::threadlib::SyncLock lock(m_mutex);

  size_t nslots = (size - 1) / sizeof(Slot) + 1;

  size_t i = (Slot*)slot - m_slots;

  for(size_t j = i; j < i+nslots; ++j)
    m_slat[j >> 6][j & 0x3F] = 0;

  RESOURCE_USE(ResourceSlot, (m_slatused -= nslots), m_slatsize)

  m_slathead = min(m_slathead, i);
}


///////////////////////// ResourceManager::token  ///////////////////////////
size_t ResourceManager::token()
{
  leap::threadlib::SyncLock lock(m_mutex);

  return m_deleterstail;
}


///////////////////////// ResourceManager::release //////////////////////////
void ResourceManager::release(size_t token)
{
  assert(m_deleterstail - m_deletershead < m_deleters.size());

  while (m_deletershead < token)
  {
    auto &holder = m_deleters[m_deletershead % m_deleters.size()];

    holder->destroy(this, holder.resource);

    ++m_deletershead;
  }
}


///////////////////////// ResourceManager::initialise_device ////////////////
void ResourceManager::initialise_device(VkPhysicalDevice physicaldevice, VkDevice device, VkQueue transferqueue, uint32_t transferqueuefamily, size_t buffersize, size_t maxbuffersize)
{
  initialise_vulkan_device(&vulkan, physicaldevice, device, transferqueue, transferqueuefamily);

  m_buffers = nullptr;
  m_buffersallocated = 0;
  m_minallocation = buffersize;
  m_maxallocation = maxbuffersize;

  assert(vulkan.physicaldeviceproperties.limits.minMemoryMapAlignment >= alignof(Buffer));

  RESOURCE_USE(ResourceBuffer, (m_bufferused = 0), m_buffersallocated)
}


///////////////////////// ResourceManager::acquire_lump /////////////////////
ResourceManager::TransferLump const *ResourceManager::acquire_lump(size_t size)
{
  leap::threadlib::SyncLock lock(m_mutex);

  size_t bytes = alignto(sizeof(Buffer), BufferAlignment) + alignto(size, BufferAlignment);

  for(size_t k = 0; k < 2; ++k)
  {
    Buffer *buffer = m_buffers;
    Buffer **into = &m_buffers;

    Buffer *basebuffer = nullptr;

    while (buffer != nullptr)
    {
      if (buffer->offset == 0)
      {
        basebuffer = buffer;
      }

      if (buffer->used + bytes <= buffer->size)
      {
        buffer = new((uint8_t*)buffer + buffer->used) Buffer;

        buffer->size = (*into)->size - (*into)->used;
        buffer->used = bytes;

        buffer->offset = (*into)->offset + (*into)->used;
        buffer->transferlump.fence = create_fence(vulkan, VK_FENCE_CREATE_SIGNALED_BIT);
        buffer->transferlump.commandpool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        buffer->transferlump.commandbuffer = allocate_commandbuffer(vulkan, buffer->transferlump.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        buffer->transferlump.transfermemory = nullptr;

        if (size != 0)
        {
          buffer->transferlump.transferbuffer = create_storagebuffer(vulkan, basebuffer->transferlump.transferbuffer.memory, buffer->offset + BufferAlignment, size);
          buffer->transferlump.transfermemory = (uint8_t*)buffer + BufferAlignment;

          assert(size <= bytes);
        }

        buffer->next = (*into)->next;

        (*into)->size = (*into)->used;
        (*into)->next = buffer;

        RESOURCE_USE(ResourceBuffer, (m_bufferused += bytes), m_buffersallocated)

        return &buffer->transferlump;
      }

      into = &buffer->next;
      buffer = buffer->next;
    }

    if (m_buffersallocated < m_maxallocation)
    {
      auto transferbuffer = create_transferbuffer(vulkan, max(bytes + BufferAlignment, m_minallocation));

      auto memory = map_memory(vulkan, transferbuffer.memory, 0, transferbuffer.size).release();

      assert(((size_t)memory & (alignof(Buffer)-1)) == 0);

      buffer = new(memory) Buffer;

      buffer->size = transferbuffer.size;
      buffer->used = alignto(sizeof(Buffer), BufferAlignment);

      buffer->offset = 0;
      buffer->transferlump.transferbuffer = std::move(transferbuffer);
      buffer->transferlump.transfermemory = nullptr;

      buffer->next = nullptr;

      *into = buffer;

      m_buffersallocated += buffer->size;

      RESOURCE_USE(ResourceBuffer, m_bufferused, m_buffersallocated)
    }
  }

  LOG_ONCE("Resource Buffers Exhausted");

  return nullptr;
}


///////////////////////// ResourceManager::release_lump /////////////////////
void ResourceManager::release_lump(TransferLump const *lump)
{ 
  leap::threadlib::SyncLock lock(m_mutex);

  Buffer *buffer = m_buffers;
  Buffer **into = &m_buffers;

  while (buffer != nullptr)
  {
    if (&buffer->next->transferlump == lump)
    {
      wait_fence(vulkan, lump->fence);

      buffer->next->~Buffer();

      RESOURCE_USE(ResourceBuffer, (m_bufferused -= buffer->next->used), m_buffersallocated)

      buffer->size += buffer->next->size;
      buffer->next = buffer->next->next;
    }

    if (buffer->offset == 0 && buffer->size == buffer->transferlump.transferbuffer.size && m_buffersallocated > m_minallocation)
    {
      m_buffersallocated -= buffer->size;

      buffer->~Buffer();

      RESOURCE_USE(ResourceBuffer, m_bufferused, m_buffersallocated)

      *into = buffer->next;
      buffer = buffer->next;

      continue;
    }

    into = &buffer->next;
    buffer = buffer->next;
  }
}


///////////////////////// ResourceManager::submit ///////////////////////////
void ResourceManager::submit(TransferLump const *lump)
{
  leap::threadlib::SyncLock lock(m_mutex);

  Vulkan::submit(vulkan, lump->commandbuffer, lump->fence);
}


///////////////////////// ResourceManager::submit ///////////////////////////
void ResourceManager::submit(VkCommandBuffer setupbuffer, VkFence fence)
{
  leap::threadlib::SyncLock lock(m_mutex);

  Vulkan::submit(vulkan, setupbuffer, fence);
}


///////////////////////// initialise_resource_system ////////////////////////
bool initialise_resource_system(DatumPlatform::PlatformInterface &platform, ResourceManager &resourcemanager, size_t slabsize, size_t buffersize, size_t maxbuffersize, uint32_t queueindex)
{
  auto renderdevice = platform.render_device();

  resourcemanager.initialise_slab(slabsize);
  resourcemanager.initialise_device(renderdevice.physicaldevice, renderdevice.device, renderdevice.queues[queueindex].queue, renderdevice.queues[queueindex].familyindex, buffersize, maxbuffersize);

  return true;
}
