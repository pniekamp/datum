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

struct SkyboxSet
{
  Color4 skycolor;
  Color4 groundcolor;
  Vec4 sundirection;
  Color3 sunintensity;
  float exposure;
};


///////////////////////// draw_skybox ///////////////////////////////////////
void draw_skybox(RenderContext &context, VkCommandBuffer commandbuffer, RenderParams const &params)
{
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


///////////////////////// ResourceManager::create ///////////////////////////
template<>
SkyBox const *ResourceManager::create<SkyBox>(int width, int height)
{
  auto slot = acquire_slot(sizeof(SkyBox));

  if (!slot)
    return nullptr;

  auto skybox = new(slot) SkyBox;

  skybox->texture = nullptr;

  skybox->envmap = create_attachment(vulkan, width, height, 6, 8, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

  VkImageViewCreateInfo viewinfo = {};
  viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewinfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
  viewinfo.format = skybox->envmap.format;
  viewinfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
  viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, skybox->envmap.levels, 0, skybox->envmap.layers };
  viewinfo.image = skybox->envmap.image;

  skybox->envmap.imageview = create_imageview(vulkan, viewinfo);

  skybox->state = SkyBox::State::Ready;

  set_slothandle(slot, nullptr);

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


//|---------------------- SkyBoxRenderer ------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// prepare_skybox_context ////////////////////////////
bool prepare_skybox_context(DatumPlatform::v1::PlatformInterface &platform, SkyboxContext &context, AssetManager *assets, uint32_t queueinstance)
{
  if (context.initialised)
    return true;

  if (context.device == 0)
  {
    //
    // Vulkan Device
    //

    auto renderdevice = platform.render_device();

    initialise_vulkan_device(&context.device, renderdevice.physicaldevice, renderdevice.device, queueinstance);

    context.fence = create_fence(context.device);

    context.commandpool = create_commandpool(context.device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    context.commandbuffer = allocate_commandbuffer(context.device, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // DescriptorPool

    VkDescriptorPoolSize typecounts[1] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    typecounts[0].descriptorCount = 1;

    VkDescriptorPoolCreateInfo descriptorpoolinfo = {};
    descriptorpoolinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorpoolinfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorpoolinfo.maxSets = accumulate(begin(typecounts), end(typecounts), 0, [](int i, auto &k) { return i + k.descriptorCount; });
    descriptorpoolinfo.poolSizeCount = extentof(typecounts);
    descriptorpoolinfo.pPoolSizes = typecounts;

    context.descriptorpool = create_descriptorpool(context.device, descriptorpoolinfo);

    // PipelineCache

    VkPipelineCacheCreateInfo pipelinecacheinfo = {};
    pipelinecacheinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    context.pipelinecache = create_pipelinecache(context.device, pipelinecacheinfo);
  }

  if (context.descriptorsetlayout == 0)
  {
    // Skybox Set

    VkDescriptorSetLayoutBinding bindings[1] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.descriptorsetlayout = create_descriptorsetlayout(context.device, createinfo);
  }

  if (context.pipelinelayout == 0)
  {
    // PipelineLayout

    VkPushConstantRange constants[1] = {};
    constants[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    constants[0].offset = 0;
    constants[0].size = sizeof(SkyboxSet);

    VkDescriptorSetLayout layouts[1] = {};
    layouts[0] = context.descriptorsetlayout;

    VkPipelineLayoutCreateInfo pipelinelayoutinfo = {};
    pipelinelayoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelinelayoutinfo.pushConstantRangeCount = extentof(constants);
    pipelinelayoutinfo.pPushConstantRanges = constants;
    pipelinelayoutinfo.setLayoutCount = extentof(layouts);
    pipelinelayoutinfo.pSetLayouts = layouts;

    context.pipelinelayout = create_pipelinelayout(context.device, pipelinelayoutinfo);
  }

  if (context.pipeline == 0)
  {
    auto cs = assets->find(CoreAsset::skybox_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.device, cssrc, cs->length);

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.pipeline = create_pipeline(context.device, context.pipelinecache, pipelineinfo);

    context.descriptorset = allocate_descriptorset(context.device, context.descriptorpool, context.descriptorsetlayout);
  }

  context.initialised = true;

  return true;
}


///////////////////////// render ////////////////////////////////////////////
void render_skybox(SkyboxContext &context, SkyBox const *skybox, SkyboxParams const &params)
{ 
  using namespace Vulkan;

  SkyboxSet skyboxset;
  skyboxset.skycolor = params.skycolor;
  skyboxset.groundcolor = params.groundcolor;
  skyboxset.sundirection.xyz = params.sundirection;
  skyboxset.sunintensity = params.sunintensity;
  skyboxset.exposure = params.exposure;

  auto &commandbuffer = context.commandbuffer;

  bindimageview(context.device, context.descriptorset, 0, skybox->envmap.imageview);

  begin(context.device, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  bindresource(commandbuffer, context.pipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  bindresource(commandbuffer, context.descriptorset, context.pipelinelayout, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

  push(commandbuffer, context.pipelinelayout, 0, sizeof(skyboxset), &skyboxset, VK_SHADER_STAGE_COMPUTE_BIT);

  dispatch(commandbuffer, (skybox->envmap.width + 15)/16, (skybox->envmap.height + 15)/16, 1);

  mip(commandbuffer, skybox->envmap.image, skybox->envmap.width, skybox->envmap.height, skybox->envmap.layers, skybox->envmap.levels);

  end(context.device, commandbuffer);

  //
  // Submit
  //

  submit(context.device, commandbuffer, context.fence);

  wait(context.device, context.fence);
}
