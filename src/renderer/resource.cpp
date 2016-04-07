//
// Datum - resources
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "resource.h"
#include "assetpack.h"
#include "debug.h"
#include <algorithm>
#include <memory>

using namespace std;


//|---------------------- ResourceManager -----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::Constructor //////////////////////
ResourceManager::ResourceManager(AssetManager *assets, allocator_type const &allocator)
  : m_allocator(allocator),
    m_blocks(allocator),
    m_slothandles(allocator),
    m_deleters(allocator),
    m_assets(assets)
{
  m_data = nullptr;
  m_deletershead = 0;
  m_deleterstail = 0;
}


///////////////////////// ResourceManager::initialise ///////////////////////
void ResourceManager::initialise(std::size_t slabsize)
{
  int nslots = slabsize / sizeof(Slot);

  m_data = ::allocate<Slot>(m_allocator, nslots);

  m_blocks.reserve(nslots / 32);

  m_blocks.push_back({ m_data, m_data + nslots });

  RESOURCE_SET(resourceblockssused, 1)
  RESOURCE_SET(resourceblockscapacity, m_blocks.capacity())

  m_slothandles.resize(nslots);

  RESOURCE_SET(resourceslotsused, 0)
  RESOURCE_SET(resourceslotscapacity, m_slothandles.capacity())

  m_deletershead = 0;
  m_deleterstail = 0;
  m_deleters.resize(nslots);

  cout << "Resource Storage: " << slabsize / 1024 / 1024 << " MiB" << endl;
}


///////////////////////// ResourceManager::acquire_slot /////////////////////
void *ResourceManager::acquire_slot(std::size_t size)
{
  leap::threadlib::SyncLock lock(m_mutex);

  int nslots = (size - 1) / sizeof(Slot) + 1;

  for(auto block = m_blocks.rbegin(); block != m_blocks.rend(); ++block)
  {
    if (nslots <= block->finish - block->start)
    {
      auto slot = block->start;

      block->start += nslots;

      if (block->start == block->finish)
      {
        m_blocks.erase(next(block).base());

        RESOURCE_RELEASE(resourceblockssused, 1)
      }

      RESOURCE_ACQUIRE(resourceslotsused, nslots)

      return slot;
    }
  }

  LOG_ONCE("Resource Slots Exhausted");

  return nullptr;
}


///////////////////////// ResourceManager::release_slot /////////////////////
void ResourceManager::release_slot(void *slot, std::size_t size)
{
  leap::threadlib::SyncLock lock(m_mutex);

  int nslots = (size - 1) / sizeof(Slot) + 1;

  if (m_blocks.size() < m_blocks.capacity())
  {
    RESOURCE_ACQUIRE(resourceblockssused, 1)
    RESOURCE_RELEASE(resourceslotsused, nslots)

    m_blocks.push_back({ static_cast<Slot*>(slot), static_cast<Slot*>(slot) + nslots });
  }

  // Merge adjacent blocks
  for(auto block1 = m_blocks.begin(); block1 != m_blocks.end(); ++block1)
  {
#if 1
    auto block2 = m_blocks.end() - 1;
#else
    for(auto block2 = next(block1); block2 != m_blocks.end(); )
#endif
    {
      if (block2->start == block1->finish)
      {
        block1->finish = block2->finish;
        block2 = m_blocks.erase(block2);

        RESOURCE_RELEASE(resourceblockssused, 1)

        continue;
      }

      if (block2->finish == block1->start)
      {
        block1->start = block2->start;
        block2 = m_blocks.erase(block2);

        RESOURCE_RELEASE(resourceblockssused, 1)

        continue;
      }

      ++block2;
    }
  }
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



///////////////////////// initialise_resource_system ////////////////////////
bool initialise_resource_system(DatumPlatform::PlatformInterface &platform, ResourceManager &resourcemanager)
{
  resourcemanager.initialise(2*1024*1024);

  return true;
}
