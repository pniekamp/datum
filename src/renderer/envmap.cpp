//
// Datum - envmap
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "envmap.h"
#include "renderer.h"
#include <leap/lml/matrixconstants.h>
#include "debug.h"

using namespace std;
using namespace lml;
using leap::alignto;
using leap::extentof;


//|---------------------- EnvMap --------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
EnvMap const *ResourceManager::create<EnvMap>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  assert(asset->layers == 6);

  auto slot = acquire_slot(sizeof(EnvMap));

  if (!slot)
    return nullptr;

  auto envmap = new(slot) EnvMap;

  envmap->width = asset->width;
  envmap->height = asset->height;
  envmap->state = EnvMap::State::Empty;

  set_slothandle(slot, asset);

  return envmap;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
EnvMap const *ResourceManager::create<EnvMap>(int width, int height)
{
  auto slot = acquire_slot(sizeof(EnvMap));

  if (!slot)
    return nullptr;

  auto envmap = new(slot) EnvMap;

  envmap->width = width;
  envmap->height = height;
  envmap->texture = create_texture(vulkan, width, height, 6, 8, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

  envmap->state = EnvMap::State::Ready;

  set_slothandle(slot, nullptr);

  return envmap;
}

template<>
EnvMap const *ResourceManager::create<EnvMap>(uint32_t width, uint32_t height)
{
  return create<EnvMap>((int)width, (int)height);
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
    auto asset = get_slothandle<Asset const *>(slot);

    if (asset)
    {
      auto bits = m_assets->request(platform, asset);

      if (bits)
      {
        size_t datasize = 0;

        for(int i = 0, width = asset->width, height = asset->height, layers = asset->layers; i < asset->levels; ++i)
        {
          datasize += width * height * layers * sizeof(uint32_t);

          width /= 2;
          height /= 2;
        }

        auto lump = acquire_lump(datasize);

        if (lump)
        {
          wait(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          slot->texture = create_texture(vulkan, asset->width, asset->height, asset->layers, asset->levels, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

          setimagelayout(lump->commandbuffer, slot->texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, slot->texture.levels, 0, slot->texture.layers });

          memcpy(lump->transfermemory, (char*)bits, lump->transferbuffer.size);

          update_texture(lump->commandbuffer, lump->transferbuffer, slot->texture);

          end(vulkan, lump->commandbuffer);

          submit_transfer(lump);

          while (!test(vulkan, lump->fence))
            ;

          release_lump(lump);

          slot->state = EnvMap::State::Ready;
        }
        else
          slot->state = EnvMap::State::Empty;
      }
      else
        slot->state = EnvMap::State::Empty;
    }
    else
      slot->state = EnvMap::State::Empty;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<EnvMap>(EnvMap const *envmap)
{
  assert(envmap);

  defer_destroy(envmap);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<EnvMap>(EnvMap const *envmap)
{
  assert(envmap);

  auto slot = const_cast<EnvMap*>(envmap);

  envmap->~EnvMap();

  release_slot(slot, sizeof(EnvMap));
}
