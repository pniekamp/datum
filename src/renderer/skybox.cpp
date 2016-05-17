//
// Datum - skybox
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "skybox.h"
#include "renderer.h"
#include "leap/lml/matrixconstants.h"
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

struct SceneSet
{
  Matrix4f modelview;

  float exposure;
};


///////////////////////// draw_skybox ///////////////////////////////////////
void draw_skybox(RenderContext &context, VkCommandBuffer commandbuffer, RenderParams const &params)
{
  using namespace Vulkan;

  assert(sizeof(SceneSet) < context.skyboxbuffersize);

  if (!params.skybox || !params.skybox->ready())
    return;

  auto &skyboxcommandbuffer = context.skyboxcommands[context.frame & 1];

  auto &skyboxdescriptor = context.skyboxbuffers[context.frame & 1];

  auto offset = context.skyboxbufferoffsets[context.frame & 1];

  auto scene = (SceneSet*)(context.transfermemory + offset);

  scene->modelview = (params.skyboxorientation * Transform::rotation(context.camera.rotation())).matrix() * inverse(context.proj);
  scene->exposure = context.camera.exposure();

  begin(context.device, skyboxcommandbuffer, context.framebuffer, context.renderpass, RenderPasses::skyboxpass, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bindresource(skyboxcommandbuffer, context.skyboxpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindtexture(context.device, skyboxdescriptor, ShaderLocation::skyboxmap, params.skybox->cubemap);

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
      auto asset = get_slothandle<Asset const *>(slot);

      slot->cubemap.width = skybox->texture->texture.width;
      slot->cubemap.height = skybox->texture->texture.height;
      slot->cubemap.layers = 1;
      slot->cubemap.levels = skybox->texture->texture.levels;
      slot->cubemap.format = skybox->texture->texture.format;

      VkSamplerCreateInfo samplerinfo = {};
      samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      samplerinfo.magFilter = VK_FILTER_LINEAR;
      samplerinfo.minFilter = VK_FILTER_LINEAR;
      samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      samplerinfo.minLod = 0.0f;
      samplerinfo.maxLod = asset->levels;
      samplerinfo.compareOp = VK_COMPARE_OP_NEVER;

      slot->cubemap.sampler = create_sampler(vulkan, samplerinfo);

      VkImageViewCreateInfo viewinfo = {};
      viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewinfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
      viewinfo.format = skybox->texture->texture.format;
      viewinfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
      viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, (uint32_t)asset->levels, 0, 1 };
      viewinfo.image = skybox->texture->texture.image;

      slot->cubemap.imageview = create_imageview(vulkan, viewinfo);

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
