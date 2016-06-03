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

  auto &skyboxcommandbuffer = context.skyboxcommands[context.frame & 1];

  auto &skyboxdescriptor = context.skyboxdescriptors[context.frame & 1];

  auto offset = context.sceneoffsets[context.frame & 1];

  begin(context.device, skyboxcommandbuffer, context.framebuffer, context.renderpass, RenderPasses::skyboxpass, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bindresource(skyboxcommandbuffer, context.skyboxpipeline, 0, context.fbocrop, context.fbowidth, context.fboheight - context.fbocrop - context.fbocrop, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindtexture(context.device, skyboxdescriptor, ShaderLocation::skyboxmap, params.skybox->envmap);

  bindresource(skyboxcommandbuffer, skyboxdescriptor, context.pipelinelayout, ShaderLocation::sceneset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindresource(skyboxcommandbuffer, context.unitquad);

  draw(skyboxcommandbuffer, context.unitquad.vertexcount, 1, 0, 0);

  end(context.device, skyboxcommandbuffer);

  execute(commandbuffer, skyboxcommandbuffer);
}


//|---------------------- Skybox --------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
Skybox const *ResourceManager::create<Skybox>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  assert(asset->layers == 6);

  auto slot = acquire_slot(sizeof(Skybox));

  if (!slot)
    return nullptr;

  auto skybox = new(slot) Skybox;

  skybox->texture = create<Texture>(asset, Texture::Format::RGBE);

  skybox->state = Skybox::State::Loading;

  set_slothandle(slot, asset);

  return skybox;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<Skybox>(DatumPlatform::PlatformInterface &platform, Skybox const *skybox)
{
  assert(skybox);
  assert(skybox->texture);

  request(platform, skybox->texture);

  if (skybox->texture->ready())
  {
    auto slot = const_cast<Skybox*>(skybox);

    Skybox::State loading = Skybox::State::Loading;

    if (slot->state.compare_exchange_strong(loading, Skybox::State::Finalising))
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

      slot->state = Skybox::State::Ready;
    }
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Skybox>(Skybox const *skybox)
{
  assert(skybox);

  defer_destroy(skybox);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Skybox>(Skybox const *skybox)
{
  assert(skybox);

  auto slot = const_cast<Skybox*>(skybox);

  destroy(skybox->texture);

  skybox->~Skybox();

  release_slot(slot, sizeof(Skybox));
}