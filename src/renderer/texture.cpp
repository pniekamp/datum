//
// Datum - texture
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "texture.h"
#include "assetpack.h"
#include "debug.h"

using namespace std;

namespace
{
  size_t image_datasize(int width, int height, int layers, int levels, VkFormat format)
  {
    size_t size = 0;
    for(int i = 0; i < levels; ++i)
    {
      switch(format)
      {
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
          size += width * height * sizeof(uint32_t) * layers;
          break;

        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
          size += ((width + 3)/4) * ((height + 3)/4) * 16 * layers;
          break;

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
  texture->asset = asset;
  texture->transferlump = nullptr;
  texture->state = Texture::State::Empty;

  return texture;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Texture const *ResourceManager::create<Texture>(int width, int height, int layers, int levels, Texture::Format format)
{
  auto slot = acquire_slot(sizeof(Texture));

  if (!slot)
    return nullptr;

  auto texture = new(slot) Texture;

  texture->width = width;
  texture->height = height;
  texture->layers = layers;
  texture->format = format;
  texture->asset = nullptr;
  texture->transferlump = nullptr;
  texture->state = Texture::State::Empty;

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

    case Texture::Format::FLOAT16:
      vkformat = VK_FORMAT_R16G16B16A16_SFLOAT;
      break;

    case Texture::Format::FLOAT32:
      vkformat = VK_FORMAT_R32G32B32A32_SFLOAT;
      break;
  }

  auto setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
  auto setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  auto setupfence = create_fence(vulkan, 0);

  begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  texture->texture = create_texture(vulkan, setupbuffer, width, height, layers, levels, vkformat);

  end(vulkan, setupbuffer);

  submit(setupbuffer, setupfence);

  wait_fence(vulkan, setupfence);

  texture->state = Texture::State::Ready;

  return texture;
}

template<>
Texture const *ResourceManager::create<Texture>(size_t width, size_t height, size_t layers, size_t levels, Texture::Format format)
{
  return create<Texture>((int)width, (int)height, (int)layers, (int)levels, format);
}

template<>
Texture const *ResourceManager::create<Texture>(uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, Texture::Format format)
{
  return create<Texture>((int)width, (int)height, (int)layers, (int)levels, format);
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Texture>(Texture const *texture, ResourceManager::TransferLump const *lump)
{
  assert(lump);
  assert(texture);
  assert(texture->state == Texture::State::Ready);

  auto slot = const_cast<Texture*>(texture);

  wait_fence(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  update_texture(lump->commandbuffer, lump->transferbuffer, 0, slot->texture);

  end(vulkan, lump->commandbuffer);

  submit(lump);

  while (!test_fence(vulkan, lump->fence))
    ;
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Texture>(Texture const *texture, ResourceManager::TransferLump const *lump, int x, int y, int w, int h, int layer, int level)
{
  assert(lump);
  assert(texture);
  assert(texture->state == Texture::State::Ready);

  auto slot = const_cast<Texture*>(texture);

  wait_fence(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  setimagelayout(lump->commandbuffer, slot->texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  blit(lump->commandbuffer, lump->transferbuffer, 0, slot->texture.image, x, y, w, h, { VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)level, (uint32_t)layer, 1 });

  setimagelayout(lump->commandbuffer, slot->texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  end(vulkan, lump->commandbuffer);

  submit(lump);

  while (!test_fence(vulkan, lump->fence))
    ;
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
    if (auto asset = slot->asset)
    {
      assert(assets()->barriercount != 0);

      if (auto bits = m_assets->request(platform, asset))
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

          case Texture::Format::FLOAT16:
            break;

          case Texture::Format::FLOAT32:
            break;
        }

        assert(vkformat != VK_FORMAT_UNDEFINED);

        if (auto lump = acquire_lump(image_datasize(asset->width, asset->height, asset->layers, asset->levels, vkformat)))
        {
          wait_fence(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          slot->texture = create_texture(vulkan, lump->commandbuffer, asset->width, asset->height, asset->layers, asset->levels, vkformat);

          memcpy(lump->memory(), bits, lump->transferbuffer.size);

          update_texture(lump->commandbuffer, lump->transferbuffer, 0, slot->texture);

          end(vulkan, lump->commandbuffer);

          submit(lump);

          slot->transferlump = lump;
        }
      }
    }

    slot->state = (slot->texture) ? Texture::State::Waiting : Texture::State::Empty;
  }

  Texture::State waiting = Texture::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, Texture::State::Testing))
  {
    bool ready = false;

    if (test_fence(vulkan, slot->transferlump->fence))
    {
      release_lump(slot->transferlump);

      slot->transferlump = nullptr;

      ready = true;
    }

    slot->state = (ready) ? Texture::State::Ready : Texture::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Texture>(Texture const *texture)
{
  defer_destroy(texture);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Texture>(Texture const *texture)
{
  if (texture)
  {
    if (texture->transferlump)
      release_lump(texture->transferlump);

    texture->~Texture();

    release_slot(const_cast<Texture*>(texture), sizeof(Texture));
  }
}
