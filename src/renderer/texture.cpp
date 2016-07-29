//
// Datum - texture
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "texture.h"
#include "resource.h"
#include "assetpack.h"
#include "debug.h"

using namespace std;

namespace
{
  size_t image_datasize(int width, int height, int layers, int levels, VkFormat format)
  {
    assert(width > 0 && height > 0 && layers > 0 && levels > 0);

    size_t size = 0;
    for(int i = 0; i < levels; ++i)
    {
      switch(format)
      {
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
          size += width * height * layers * sizeof(uint32_t);
          break;

        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
          size += ((width + 3)/4) * ((height + 3)/4) * layers * 16;
          break;

        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
          size += width * height * layers * sizeof(uint32_t);
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

  size_t image_datasize(Vulkan::Texture const &texture)
  {
    return image_datasize(texture.width, texture.height, texture.layers, texture.levels, texture.format);
  }
}

//|---------------------- Texture -------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
Texture const *ResourceManager::create<Texture>(Asset const *asset, Texture::Format format)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(Texture));

  if (!slot)
    return nullptr;

  auto texture = new(slot) Texture;

  texture->width = asset->width;
  texture->height = asset->height;
  texture->layers = asset->layers;
  texture->format = format;
  texture->memory = nullptr;
  texture->transferlump = nullptr;
  texture->state = Texture::State::Empty;

  set_slothandle(slot, asset);

  return texture;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Texture const *ResourceManager::create<Texture>(int width, int height, int layers, int levels, Texture::Format format)
{
  auto slot = acquire_slot(sizeof(Texture));

  if (!slot)
    return nullptr;

  VkFormat vkformat = VK_FORMAT_UNDEFINED;

  switch(format)
  {
    case Texture::Format::RGBA:
      vkformat = VK_FORMAT_B8G8R8A8_UNORM;
      break;

    case Texture::Format::SRGBA:
      vkformat = VK_FORMAT_B8G8R8A8_SRGB;
      break;

    case Texture::Format::RGBM:
      vkformat = VK_FORMAT_B8G8R8A8_UNORM;
      break;

    case Texture::Format::RGBE:
      vkformat = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
      break;
  }

  auto lump = acquire_lump(image_datasize(width, height, layers, levels, vkformat));

  if (!lump)
    return nullptr;

  auto texture = new(slot) Texture;

  texture->width = width;
  texture->height = height;
  texture->layers = layers;
  texture->format = format;
  texture->transferlump = lump;

  wait(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  texture->texture = create_texture(vulkan, lump->commandbuffer, width, height, layers, levels, vkformat);

  end(vulkan, lump->commandbuffer);

  submit_transfer(lump);

  texture->memory = (uint32_t*)lump->transfermemory;

  texture->state = Texture::State::Ready;

  set_slothandle(slot, nullptr);

  return texture;
}

template<>
Texture const *ResourceManager::create<Texture>(uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, Texture::Format format)
{
  return create<Texture>((int)width, (int)height, (int)layers, (int)levels, format);
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Texture>(Texture const *texture)
{
  assert(texture);
  assert(texture->memory);
  assert(texture->state == Texture::State::Ready);

  auto slot = const_cast<Texture*>(texture);

  auto lump = slot->transferlump;

  wait(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  update_texture(lump->commandbuffer, lump->transferbuffer, slot->texture);

  end(vulkan, lump->commandbuffer);

  submit_transfer(lump);

  while (!test(vulkan, lump->fence))
    ;
}

template<>
void ResourceManager::update<Texture>(Texture const *texture, void const *bits)
{
  assert(texture);
  assert(texture->memory);
  assert(texture->state == Texture::State::Ready);

  auto slot = const_cast<Texture*>(texture);

  memcpy(slot->memory, bits, image_datasize(slot->texture));

  update(texture);
}

template<>
void ResourceManager::update<Texture>(Texture const *texture, void *bits)
{
  update<Texture>(texture, (void const *)bits);
}

template<>
void ResourceManager::update<Texture>(Texture const *texture, uint32_t *bits)
{
  update<Texture>(texture, (void const *)bits);
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<Texture>(DatumPlatform::PlatformInterface &platform, Texture const *texture)
{
  assert(texture);

  auto slot = const_cast<Texture*>(texture);

  Texture::State empty = Texture::State::Empty;

  if (slot->state.compare_exchange_strong(empty, Texture::State::Loading))
  {
    auto asset = get_slothandle<Asset const *>(slot);

    if (asset)
    {
      auto bits = m_assets->request(platform, asset);

      if (bits)
      {
        VkFormat vkformat = VK_FORMAT_UNDEFINED;

        switch(slot->format)
        {
          case Texture::Format::RGBA:
            if (asset->format == PackImageHeader::rgba)
              vkformat = VK_FORMAT_B8G8R8A8_UNORM;
            if (asset->format == PackImageHeader::rgba_bc3)
              vkformat = VK_FORMAT_BC3_UNORM_BLOCK;
            break;

          case Texture::Format::SRGBA:
            if (asset->format == PackImageHeader::rgba)
              vkformat = VK_FORMAT_B8G8R8A8_SRGB;
            if (asset->format == PackImageHeader::rgba_bc3)
              vkformat = VK_FORMAT_BC3_SRGB_BLOCK;
            break;

          case Texture::Format::RGBM:
            if (asset->format == PackImageHeader::rgba)
              vkformat = VK_FORMAT_B8G8R8A8_UNORM;
            if (asset->format == PackImageHeader::rgba_bc3)
              vkformat = VK_FORMAT_BC3_UNORM_BLOCK;
            break;

          case Texture::Format::RGBE:
            if (asset->format == PackImageHeader::rgbe)
              vkformat = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
            break;
        }

        assert(vkformat != VK_FORMAT_UNDEFINED);

        auto lump = acquire_lump(image_datasize(asset->width, asset->height, asset->layers, asset->levels, vkformat));

        if (lump)
        {
          slot->transferlump = lump;

          wait(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          slot->texture = create_texture(vulkan, lump->commandbuffer, asset->width, asset->height, asset->layers, asset->levels, vkformat);

          memcpy(lump->transfermemory, (char*)bits, lump->transferbuffer.size);

          update_texture(lump->commandbuffer, lump->transferbuffer, slot->texture);

          end(vulkan, lump->commandbuffer);

          submit_transfer(lump);

          slot->state = Texture::State::Waiting;
        }
        else
          slot->state = Texture::State::Empty;
      }
      else
        slot->state = Texture::State::Empty;
    }
    else
      slot->state = Texture::State::Empty;
  }

  Texture::State waiting = Texture::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, Texture::State::Testing))
  {
    if (test(vulkan, slot->transferlump->fence))
    {
      release_lump(slot->transferlump);

      slot->transferlump = nullptr;

      slot->state = Texture::State::Ready;
    }
    else
      slot->state = Texture::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Texture>(Texture const *texture)
{
  assert(texture);

  defer_destroy(texture);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Texture>(Texture const *texture)
{
  assert(texture);

  auto slot = const_cast<Texture*>(texture);

  if (slot->transferlump)
    release_lump(slot->transferlump);

  texture->~Texture();

  release_slot(slot, sizeof(Texture));
}
