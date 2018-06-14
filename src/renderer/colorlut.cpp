//
// Datum - colorlut
//

//
// Copyright (c) 2018 Peter Niekamp
//

#include "colorlut.h"
#include "assetpack.h"
#include "renderer.h"
#include <numeric>
#include "debug.h"

using namespace std;
using namespace lml;
using namespace Vulkan;

namespace
{
  size_t image_datasize(int width, int height, int depth, VkFormat format)
  {
    assert(width > 0 && height > 0 && depth > 0);

    switch(format)
    {
      case VK_FORMAT_B8G8R8A8_UNORM:
        return width * height * depth * sizeof(uint32_t);

      case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
        return width * height * depth * sizeof(uint32_t);

      case VK_FORMAT_R16G16B16A16_SFLOAT:
        return width * height * depth * 4*sizeof(uint16_t);

      case VK_FORMAT_R32G32B32A32_SFLOAT:
        return width * height * depth * 4*sizeof(uint32_t);

      default:
        assert(false);
    }

    return 0;
  }
}


//|---------------------- ColorLut ------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
ColorLut const *ResourceManager::create<ColorLut>(Asset const *asset, ColorLut::Format format)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(ColorLut));

  if (!slot)
    return nullptr;

  auto colorlut = new(slot) ColorLut;

  colorlut->width = asset->width;
  colorlut->height = asset->height;
  colorlut->depth = asset->layers;
  colorlut->format = format;
  colorlut->asset = asset;
  colorlut->transferlump = nullptr;
  colorlut->state = ColorLut::State::Empty;

  return colorlut;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<ColorLut>(DatumPlatform::PlatformInterface &platform, ColorLut const *colorlut)
{
  assert(colorlut);

  auto slot = const_cast<ColorLut*>(colorlut);

  ColorLut::State empty = ColorLut::State::Empty;

  if (slot->state.compare_exchange_strong(empty, ColorLut::State::Loading))
  {
    if (auto asset = slot->asset)
    {
      assert(assets()->barriercount != 0);

      if (auto bits = m_assets->request(platform, asset))
      {
        VkFormat vkformat = VK_FORMAT_UNDEFINED;

        switch(slot->format)
        {
          case ColorLut::Format::RGBA:
            if (asset->format == PackImageHeader::rgba)
              vkformat = VK_FORMAT_B8G8R8A8_UNORM;
            break;

          case ColorLut::Format::RGBE:
            if (asset->format == PackImageHeader::rgbe)
              vkformat = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
            break;
        }

        assert(vkformat != VK_FORMAT_UNDEFINED);

        if (auto lump = acquire_lump(image_datasize(asset->width, asset->height, asset->layers, vkformat)))
        {
          wait_fence(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          if (create_texture(vulkan, lump->commandbuffer, asset->width, asset->height, asset->layers, 1, vkformat, VK_IMAGE_VIEW_TYPE_3D, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &slot->texture))
          {
            memcpy(lump->memory(), bits, lump->transferbuffer.size);
            setimagelayout(lump->commandbuffer, slot->texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            blit(lump->commandbuffer, lump->transferbuffer, 0, slot->texture.image, 0, 0, 0, asset->width, asset->height, asset->layers, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 });
            setimagelayout(lump->commandbuffer, slot->texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
          }

          end(vulkan, lump->commandbuffer);

          submit(lump);

          if (!slot->texture)
          {
            release_lump(lump);
          }
          else
          {
            slot->transferlump = lump;
          }
        }
      }
    }

    slot->state = (slot->texture) ? ColorLut::State::Waiting : ColorLut::State::Empty;
  }

  ColorLut::State waiting = ColorLut::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, ColorLut::State::Testing))
  {
    bool ready = false;

    if (test_fence(vulkan, slot->transferlump->fence))
    {
      release_lump(slot->transferlump);

      slot->transferlump = nullptr;

      ready = true;
    }

    slot->state = (ready) ? ColorLut::State::Ready : ColorLut::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<ColorLut>(ColorLut const *colorlut)
{
  defer_destroy(colorlut);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<ColorLut>(ColorLut const *colorlut)
{
  if (colorlut)
  {
    if (colorlut->transferlump)
      release_lump(colorlut->transferlump);

    colorlut->~ColorLut();

    release_slot(const_cast<ColorLut*>(colorlut), sizeof(ColorLut));
  }
}
