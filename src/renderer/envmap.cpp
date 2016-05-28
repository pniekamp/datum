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

  envmap->texture = create<Texture>(asset, Texture::Format::RGBE);

  envmap->state = EnvMap::State::Loading;

  set_slothandle(slot, asset);

  return envmap;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<EnvMap>(DatumPlatform::PlatformInterface &platform, EnvMap const *envmap)
{
  assert(envmap);
  assert(envmap->texture);

  request(platform, envmap->texture);

  if (envmap->texture->ready())
  {
    auto slot = const_cast<EnvMap*>(envmap);

    EnvMap::State loading = EnvMap::State::Loading;

    if (slot->state.compare_exchange_strong(loading, EnvMap::State::Finalising))
    {
      slot->envmap.width = envmap->texture->texture.width;
      slot->envmap.height = envmap->texture->texture.height;
      slot->envmap.layers = envmap->texture->texture.layers;
      slot->envmap.levels = envmap->texture->texture.levels;
      slot->envmap.format = envmap->texture->texture.format;

      VkSamplerCreateInfo samplerinfo = {};
      samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      samplerinfo.magFilter = VK_FILTER_LINEAR;
      samplerinfo.minFilter = VK_FILTER_LINEAR;
      samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      samplerinfo.minLod = 0.0f;
      samplerinfo.maxLod = envmap->envmap.levels;
      samplerinfo.compareOp = VK_COMPARE_OP_NEVER;

      slot->envmap.sampler = create_sampler(vulkan, samplerinfo);

      VkImageViewCreateInfo viewinfo = {};
      viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewinfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
      viewinfo.format = envmap->texture->texture.format;
      viewinfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
      viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, envmap->envmap.levels, 0, envmap->envmap.layers };
      viewinfo.image = envmap->texture->texture.image;

      slot->envmap.imageview = create_imageview(vulkan, viewinfo);

      slot->state = EnvMap::State::Ready;
    }
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

  destroy(envmap->texture);

  envmap->~EnvMap();

  release_slot(slot, sizeof(EnvMap));
}
