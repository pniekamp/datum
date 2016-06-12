//
// Datum - skybox
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "skybox.h"
#include "renderer.h"
#include <leap/lml/matrixconstants.h>
#include "debug.h"

using namespace std;
using namespace lml;
using leap::alignto;
using leap::extentof;

enum RenderPasses
{
  skyboxpass = 0,
};

enum ShaderLocation
{
  sceneset = 0,

  skyboxmap = 1,
};


///////////////////////// draw_skybox ///////////////////////////////////////
void draw_skybox(RenderContext &context, VkCommandBuffer commandbuffer, RenderParams const &params)
{
  using namespace Vulkan;

  if (!params.skybox || !params.skybox->ready())
    return;

  auto &skyboxcommands = context.skyboxcommands[context.frame & 1];

  auto &skyboxdescriptor = context.skyboxdescriptors[context.frame & 1];

  begin(context.device, skyboxcommands, context.framebuffer, context.renderpass, RenderPasses::skyboxpass, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bindresource(skyboxcommands, context.skyboxpipeline, context.fbox, context.fboy, context.fbowidth - 2*context.fbox, context.fboheight - 2*context.fboy, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindtexture(context.device, skyboxdescriptor, ShaderLocation::skyboxmap, params.skybox->envmap);

  bindresource(skyboxcommands, skyboxdescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(skyboxcommands, context.unitquad);

  draw(skyboxcommands, context.unitquad.vertexcount, 1, 0, 0);

  end(context.device, skyboxcommands);

  execute(commandbuffer, skyboxcommands);
}


//|---------------------- SkyBox --------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
SkyBox const *ResourceManager::create<SkyBox>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  assert(asset->layers == 6);

  auto slot = acquire_slot(sizeof(SkyBox));

  if (!slot)
    return nullptr;

  auto skybox = new(slot) SkyBox;

  skybox->texture = create<Texture>(asset, Texture::Format::RGBE);

  skybox->state = SkyBox::State::Loading;

  set_slothandle(slot, asset);

  return skybox;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<SkyBox>(DatumPlatform::PlatformInterface &platform, SkyBox const *skybox)
{
  assert(skybox);
  assert(skybox->texture);

  request(platform, skybox->texture);

  if (skybox->texture->ready())
  {
    auto slot = const_cast<SkyBox*>(skybox);

    SkyBox::State loading = SkyBox::State::Loading;

    if (slot->state.compare_exchange_strong(loading, SkyBox::State::Finalising))
    {
      slot->envmap.width = skybox->texture->texture.width;
      slot->envmap.height = skybox->texture->texture.height;
      slot->envmap.layers = skybox->texture->texture.layers;
      slot->envmap.levels = skybox->texture->texture.levels;
      slot->envmap.format = skybox->texture->texture.format;

      VkSamplerCreateInfo samplerinfo = {};
      samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      samplerinfo.magFilter = VK_FILTER_LINEAR;
      samplerinfo.minFilter = VK_FILTER_LINEAR;
      samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      samplerinfo.minLod = 0.0f;
      samplerinfo.maxLod = skybox->envmap.levels;
      samplerinfo.compareOp = VK_COMPARE_OP_NEVER;

      slot->envmap.sampler = create_sampler(vulkan, samplerinfo);

      VkImageViewCreateInfo viewinfo = {};
      viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewinfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
      viewinfo.format = skybox->texture->texture.format;
      viewinfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
      viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, skybox->envmap.levels, 0, skybox->envmap.layers };
      viewinfo.image = skybox->texture->texture.image;

      slot->envmap.imageview = create_imageview(vulkan, viewinfo);

      slot->state = SkyBox::State::Ready;
    }
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<SkyBox>(SkyBox const *skybox)
{
  assert(skybox);

  defer_destroy(skybox);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<SkyBox>(SkyBox const *skybox)
{
  assert(skybox);

  auto slot = const_cast<SkyBox*>(skybox);

  destroy(skybox->texture);

  skybox->~SkyBox();

  release_slot(slot, sizeof(SkyBox));
}
