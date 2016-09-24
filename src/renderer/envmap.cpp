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
  envmap->format = EnvMap::Format::RGBE;
  envmap->asset = asset;
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

  if (auto lump = acquire_lump(0))
  {
    wait(vulkan, lump->fence);

    begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    envmap->texture = create_texture(vulkan, width, height, 6, 8, vkformat, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    setimagelayout(lump->commandbuffer, envmap->texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, envmap->texture.levels, 0, envmap->texture.layers });

    end(vulkan, lump->commandbuffer);

    submit_transfer(lump);

    release_lump(lump);

    envmap->state = EnvMap::State::Ready;
  }

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

  wait(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  update_texture(lump->commandbuffer, lump->transferbuffer, slot->texture);

  end(vulkan, lump->commandbuffer);

  submit_transfer(lump);

  while (!test(vulkan, lump->fence))
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
      if (auto bits = m_assets->request(platform, asset))
      {
        size_t datasize = 0;

        for(int i = 0, width = asset->width, height = asset->height, layers = asset->layers; i < asset->levels; ++i)
        {
          datasize += width * height * layers * sizeof(uint32_t);

          width /= 2;
          height /= 2;
        }

        if (auto lump = acquire_lump(datasize))
        {
          wait(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          slot->texture = create_texture(vulkan, asset->width, asset->height, asset->layers, asset->levels, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

          setimagelayout(lump->commandbuffer, slot->texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, slot->texture.levels, 0, slot->texture.layers });

          memcpy(lump->transfermemory, (char*)bits, lump->transferbuffer.size);

          update_texture(lump->commandbuffer, lump->transferbuffer, slot->texture);

          end(vulkan, lump->commandbuffer);

          submit_transfer(lump);

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
