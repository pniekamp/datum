//
// Datum - resources
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "resource.h"
#include "assetpack.h"
#include "vulkan.h"
#include "leap/util.h"
#include <algorithm>
#include <memory>
#include "debug.h"

using namespace std;
using namespace Vulkan;
using leap::alignto;
using leap::extentof;

const size_t kBufferAlignment = 4096;


//|---------------------- ResourceManager -----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::Constructor //////////////////////
ResourceManager::ResourceManager(AssetManager *assets, allocator_type const &allocator)
  : m_allocator(allocator),
    m_slat(allocator),
    m_slothandles(allocator),
    m_deleters(allocator),
    m_assets(assets)
{
  m_slots = nullptr;
  m_slathead = 0;
  m_deletershead = 0;
  m_deleterstail = 0;

  m_buffers = nullptr;
}


///////////////////////// ResourceManager::initialise ///////////////////////
void ResourceManager::initialise_slab(size_t slabsize)
{
  int nslots = slabsize / sizeof(Slot);

  m_slots = ::allocate<Slot>(m_allocator, nslots);

  m_slat.resize(nslots);

  m_slothandles.resize(nslots);

  RESOURCE_SET(resourceslotsused, 0)
  RESOURCE_SET(resourceslotscapacity, m_slat.size())

  m_deletershead = 0;
  m_deleterstail = 0;
  m_deleters.resize(nslots);

  cout << "Resource Storage: " << slabsize / 1024 / 1024 << " MiB" << endl;
}


///////////////////////// ResourceManager::acquire_slot /////////////////////
void *ResourceManager::acquire_slot(size_t size)
{
  leap::threadlib::SyncLock lock(m_mutex);

  int nslots = (size - 1) / sizeof(Slot) + 1;

  for(size_t k = 0; k < 2; ++k)
  {
    for(size_t i = m_slathead; i < m_slat.size() - nslots; ++i)
    {
      bool available = (m_slat[i] == 0);

      for(size_t j = i+1; available && j < i+nslots; ++j)
        available &= (m_slat[j] == 0);

      if (available)
      {
        for(size_t j = i; j < i+nslots; ++j)
          m_slat[j] = 1;

        RESOURCE_ACQUIRE(resourceslotsused, nslots)

        m_slathead = i + nslots;

        return m_slots + i;
      }
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

  int nslots = (size - 1) / sizeof(Slot) + 1;

  size_t i = (Slot*)slot - m_slots;

  for(size_t j = i; j < i+nslots; ++j)
    m_slat[j] = 0;

  RESOURCE_RELEASE(resourceslotsused, nslots)

  m_slathead = i;
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
void ResourceManager::initialise_device(VkPhysicalDevice physicaldevice, VkDevice device, int queueinstance, size_t buffersize, size_t maxbuffersize)
{
  initialise_vulkan_device(&vulkan, physicaldevice, device, queueinstance);

  m_buffers = nullptr;
  m_buffersallocated = 0;
  m_minallocation = buffersize;
  m_maxallocation = maxbuffersize;

  RESOURCE_SET(resourcebufferused, 0)
  RESOURCE_SET(resourcebuffercapacity, 0)
}


///////////////////////// ResourceManager::acquire_lump /////////////////////
ResourceManager::TransferLump const *ResourceManager::acquire_lump(size_t size)
{
  leap::threadlib::SyncLock lock(m_mutex);

  size_t bytes = alignto(size + alignto(sizeof(Buffer), kBufferAlignment), kBufferAlignment);

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
        if (buffer->used != 0)
        {
          buffer = new((uint8_t*)buffer + buffer->used) Buffer;

          buffer->size = (*into)->size - (*into)->used;
          buffer->offset = (*into)->offset + (*into)->used;

          buffer->used = bytes;
          buffer->transferlump.fence = create_fence(vulkan, VK_FENCE_CREATE_SIGNALED_BIT);
          buffer->transferlump.commandpool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
          buffer->transferlump.commandbuffer = allocate_commandbuffer(vulkan, buffer->transferlump.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
          buffer->transferlump.transferbuffer = create_transferbuffer(vulkan, basebuffer->transferlump.transferbuffer.memory, buffer->offset + kBufferAlignment, bytes);
          buffer->transferlump.transfermemory = (uint8_t*)buffer + kBufferAlignment;

          buffer->next = (*into)->next;

          (*into)->size = (*into)->used;
          (*into)->next = buffer;
        }

        RESOURCE_ACQUIRE(resourcebufferused, bytes)

        return &buffer->transferlump;;
      }

      into = &buffer->next;
      buffer = buffer->next;
    }

    if (m_buffersallocated < m_maxallocation)
    {
      TransferBuffer transferbuffer = create_transferbuffer(vulkan, max(bytes + kBufferAlignment, m_minallocation));

      buffer = new(map_memory<Buffer>(vulkan, transferbuffer, 0, transferbuffer.size)) Buffer;

      buffer->size = transferbuffer.size;
      buffer->offset = 0;

      buffer->used = kBufferAlignment;
      buffer->transferlump.transferbuffer = std::move(transferbuffer);
      buffer->transferlump.transfermemory = nullptr;

      buffer->next = nullptr;

      *into = buffer;

      m_buffersallocated += buffer->size;

      RESOURCE_ACQUIRE(resourcebuffercapacity, buffer->size)
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
      wait(vulkan, lump->fence);

      buffer->next->~Buffer();

      RESOURCE_RELEASE(resourcebufferused, buffer->next->used);

      buffer->size += buffer->next->size;
      buffer->next = buffer->next->next;
    }

    if (buffer->offset == 0 && buffer->size == buffer->transferlump.transferbuffer.size && m_buffersallocated > m_minallocation)
    {
      buffer->~Buffer();

      RESOURCE_RELEASE(resourcebuffercapacity, buffer->size)

      *into = buffer->next;
      buffer = buffer->next;

      continue;
    }

    into = &buffer->next;
    buffer = buffer->next;
  }
}


///////////////////////// ResourceManager::submit_transfer //////////////////
void ResourceManager::submit_transfer(TransferLump const *lump)
{
  leap::threadlib::SyncLock lock(m_mutex);

  submit(vulkan, lump->commandbuffer, lump->fence);
}


///////////////////////// initialise_resource_system ////////////////////////
bool initialise_resource_system(DatumPlatform::PlatformInterface &platform, ResourceManager &resourcemanager, size_t slabsize, size_t buffersize, size_t maxbuffersize)
{
  auto renderdevice = platform.render_device();

  resourcemanager.initialise_slab(slabsize);
  resourcemanager.initialise_device(renderdevice.physicaldevice, renderdevice.device, 1, buffersize, maxbuffersize);

  return true;
}