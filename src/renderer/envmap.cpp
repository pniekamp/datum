//
// Datum - envmap
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "envmap.h"
#include "assetpack.h"
#include "debug.h"

using namespace std;
using namespace lml;
using leap::alignto;
using leap::extentof;

namespace
{
  size_t envmap_datasize(int width, int height, int layers, int levels, VkFormat format)
  {
    assert(width > 0 && height > 0 && layers > 0 && levels > 0);

    size_t size = 0;
    for(int i = 0; i < levels; ++i)
    {
      switch(format)
      {
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
          size += width * height * sizeof(uint32_t) * layers;
          break;

        case VK_FORMAT_R16G16B16A16_SFLOAT:
          size += width * height * 4*sizeof(uint16_t) * layers;
          break;

        case VK_FORMAT_R32G32B32A32_SFLOAT:
          size += width * height * 4*sizeof(uint32_t) * layers;
          break;

        default:
          assert(false);
          break;
      }

      width /= 2;
      height /= 2;
    }

    return size;
  }
}


//|---------------------- EnvMap --------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
EnvMap const *ResourceManager::create<EnvMap>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  assert(asset->layers == 6);
  assert(asset->format == PackImageHeader::rgbe);

  auto slot = acquire_slot(sizeof(EnvMap));

  if (!slot)
    return nullptr;

  auto envmap = new(slot) EnvMap;

  envmap->width = asset->width;
  envmap->height = asset->height;
  envmap->format = EnvMap::Format::RGBE;
  envmap->asset = asset;
  envmap->transferlump = nullptr;
  envmap->state = EnvMap::State::Empty;

  return envmap;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
EnvMap const *ResourceManager::create<EnvMap>(int width, int height, EnvMap::Format format)
{
  auto slot = acquire_slot(sizeof(EnvMap));

  if (!slot)
    return nullptr;

  auto envmap = new(slot) EnvMap;

  envmap->width = width;
  envmap->height = height;
  envmap->format = format;
  envmap->asset = nullptr;
  envmap->transferlump = nullptr;
  envmap->state = EnvMap::State::Empty;

  VkFormat vkformat = VK_FORMAT_UNDEFINED;

  switch(format)
  {
    case EnvMap::Format::RGBE:
      vkformat = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
      break;

    case EnvMap::Format::FLOAT16:
      vkformat = VK_FORMAT_R16G16B16A16_SFLOAT;
      break;

    case EnvMap::Format::FLOAT32:
      vkformat = VK_FORMAT_R32G32B32A32_SFLOAT;
      break;
  }

  {
    leap::threadlib::SyncLock lock(m_mutex);

    auto setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto setupfence = create_fence(vulkan, 0);

    begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    envmap->texture = create_texture(vulkan, setupbuffer, width, height, 6, 8, vkformat, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    end(vulkan, setupbuffer);

    submit(vulkan, setupbuffer, setupfence);

    wait_fence(vulkan, setupfence);
  }

  envmap->state = EnvMap::State::Ready;

  return envmap;
}

template<>
EnvMap const *ResourceManager::create<EnvMap>(uint32_t width, uint32_t height, EnvMap::Format format)
{
  return create<EnvMap>((int)width, (int)height, format);
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<EnvMap>(EnvMap const *envmap, ResourceManager::TransferLump const *lump)
{
  assert(lump);
  assert(envmap);
  assert(envmap->state == EnvMap::State::Ready);

  auto slot = const_cast<EnvMap*>(envmap);

  wait_fence(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  update_texture(lump->commandbuffer, lump->transferbuffer, 0, slot->texture);

  end(vulkan, lump->commandbuffer);

  submit_transfer(lump);

  while (!test_fence(vulkan, lump->fence))
    ;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<EnvMap>(DatumPlatform::PlatformInterface &platform, EnvMap const *envmap)
{
  assert(envmap);

  auto slot = const_cast<EnvMap*>(envmap);

  EnvMap::State empty = EnvMap::State::Empty;

  if (slot->state.compare_exchange_strong(empty, EnvMap::State::Loading))
  {
    if (auto asset = slot->asset)
    {
      assert(assets()->barriercount != 0);

      if (auto bits = m_assets->request(platform, asset))
      {
        VkFormat vkformat = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;

        if (auto lump = acquire_lump(envmap_datasize(asset->width, asset->height, asset->layers, asset->levels, vkformat)))
        {
          wait_fence(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          slot->texture = create_texture(vulkan, lump->commandbuffer, asset->width, asset->height, asset->layers, asset->levels, vkformat, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

          memcpy(lump->memory(), bits, lump->transferbuffer.size);

          update_texture(lump->commandbuffer, lump->transferbuffer, 0, slot->texture);

          end(vulkan, lump->commandbuffer);

          submit_transfer(lump);

          slot->transferlump = lump;
        }
      }
    }

    slot->state = (slot->texture) ? EnvMap::State::Waiting : EnvMap::State::Empty;
  }

  EnvMap::State waiting = EnvMap::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, EnvMap::State::Testing))
  {
    bool ready = false;

    if (test_fence(vulkan, slot->transferlump->fence))
    {
      release_lump(slot->transferlump);

      slot->transferlump = nullptr;

      ready = true;
    }

    slot->state = (ready) ? EnvMap::State::Ready : EnvMap::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<EnvMap>(EnvMap const *envmap)
{
  defer_destroy(envmap);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<EnvMap>(EnvMap const *envmap)
{
  if (envmap)
  {
    if (envmap->transferlump)
      release_lump(envmap->transferlump);

    envmap->~EnvMap();

    release_slot(const_cast<EnvMap*>(envmap), sizeof(EnvMap));
  }
}
