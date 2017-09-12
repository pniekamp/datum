//
// Datum - skybox
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "skybox.h"
#include "renderer.h"
#include "assetpack.h"
#include <numeric>
#include "debug.h"

using namespace std;
using namespace lml;
using leap::alignto;
using leap::extentof;

enum SkyboxFlags
{
  SkyboxOwnsTexture = 0x01,
};

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
  alignas(16) Color3 skycolor;
  alignas(16) Color3 groundcolor;
  alignas(16) Vec3 sundirection;
  alignas(16) Color3 sunintensity;
  alignas( 4) float exposure;
  alignas( 4) uint32_t cloudlayers;
  alignas( 4) float cloudheight;
  alignas(16) Color4 cloudcolor;
};

struct ConvolveSet
{
  alignas( 4) uint32_t level;
  alignas( 4) uint32_t samples;
  alignas( 4) float roughness;
};


//|---------------------- SkyBox --------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
SkyBox const *ResourceManager::create<SkyBox>(Asset const *asset)
{
  assert(sizeof(SkyBox) == sizeof(EnvMap));

  return (SkyBox const *)create<EnvMap>(asset);
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
SkyBox const *ResourceManager::create<SkyBox>(int width, int height, EnvMap::Format format)
{
  assert(sizeof(SkyBox) == sizeof(EnvMap));

  return (SkyBox const *)create<EnvMap>(width, height, format);
}


template<>
SkyBox const *ResourceManager::create<SkyBox>(uint32_t width, uint32_t height, EnvMap::Format format)
{
  return create<SkyBox>((int)width, (int)height, format);
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<SkyBox>(SkyBox const *skybox, ResourceManager::TransferLump const *lump)
{
  update<EnvMap>(skybox, lump);
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<SkyBox>(DatumPlatform::PlatformInterface &platform, SkyBox const *skybox)
{
  request<EnvMap>(platform, skybox);
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<SkyBox>(SkyBox const *skybox)
{
  defer_destroy(skybox);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<SkyBox>(SkyBox const *skybox)
{
  destroy<EnvMap>(skybox);
}


//|---------------------- SkyBoxRenderer ------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// initialise_skybox_context //////////////////////////
void initialise_skybox_context(DatumPlatform::PlatformInterface &platform, SkyBoxContext &context, uint32_t queueindex)
{
  //
  // Vulkan Device
  //

  auto renderdevice = platform.render_device();

  initialise_vulkan_device(&context.vulkan, renderdevice.physicaldevice, renderdevice.device, renderdevice.queues[queueindex].queue, renderdevice.queues[queueindex].familyindex);

  context.commandpool = create_commandpool(context.vulkan, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  context.commandbuffer = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  context.fence = create_fence(context.vulkan, VK_FENCE_CREATE_SIGNALED_BIT);

  context.rendercomplete = create_semaphore(context.vulkan);
}


///////////////////////// prepare_skybox_context ////////////////////////////
bool prepare_skybox_context(DatumPlatform::PlatformInterface &platform, SkyBoxContext &context, AssetManager &assets)
{
  if (context.ready)
    return true;

  assert(context.vulkan);

  if (context.descriptorpool == 0)
  {
    // DescriptorPool

    VkDescriptorPoolSize typecounts[2] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[0].descriptorCount = 24;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    typecounts[1].descriptorCount = 8;

    VkDescriptorPoolCreateInfo descriptorpoolinfo = {};
    descriptorpoolinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorpoolinfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorpoolinfo.maxSets = accumulate(begin(typecounts), end(typecounts), 0, [](int i, auto &k) { return i + k.descriptorCount; });
    descriptorpoolinfo.poolSizeCount = extentof(typecounts);
    descriptorpoolinfo.pPoolSizes = typecounts;

    context.descriptorpool = create_descriptorpool(context.vulkan, descriptorpoolinfo);
  }

  if (context.pipelinecache == 0)
  {
    // PipelineCache

    VkPipelineCacheCreateInfo pipelinecacheinfo = {};
    pipelinecacheinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    context.pipelinecache = create_pipelinecache(context.vulkan, pipelinecacheinfo);
  }

  if (context.descriptorsetlayout == 0)
  {
    // Skybox Set

    VkDescriptorSetLayoutBinding bindings[4] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[2].binding = 2;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[3].binding = 3;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.descriptorsetlayout = create_descriptorsetlayout(context.vulkan, createinfo);
  }

  if (context.pipelinelayout == 0)
  {
    // PipelineLayout

    VkPushConstantRange constants[1] = {};
    constants[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    constants[0].offset = 0;
    constants[0].size = max(sizeof(SkyboxSet), sizeof(ConvolveSet));

    VkDescriptorSetLayout layouts[1] = {};
    layouts[0] = context.descriptorsetlayout;

    VkPipelineLayoutCreateInfo pipelinelayoutinfo = {};
    pipelinelayoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelinelayoutinfo.pushConstantRangeCount = extentof(constants);
    pipelinelayoutinfo.pPushConstantRanges = constants;
    pipelinelayoutinfo.setLayoutCount = extentof(layouts);
    pipelinelayoutinfo.pSetLayouts = layouts;

    context.pipelinelayout = create_pipelinelayout(context.vulkan, pipelinelayoutinfo);
  }

  if (context.skyboxpipeline == 0)
  {
    auto cs = assets.find(CoreAsset::skybox_gen_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.skyboxpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);

    context.skyboxdescriptor = allocate_descriptorset(context.vulkan, context.descriptorpool, context.descriptorsetlayout);
  }

  if (context.convolvepipeline == 0)
  {
    auto cs = assets.find(CoreAsset::convolve_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.convolvepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);

    for(size_t i = 1; i < extentof(context.convolvedescriptors); ++i)
    {
      context.convolvedescriptors[i] = allocate_descriptorset(context.vulkan, context.descriptorpool, context.descriptorsetlayout);
    }
  }

  context.ready = true;

  return true;
}


///////////////////////// render ////////////////////////////////////////////
void render_skybox(SkyBoxContext &context, SkyBox const *target, SkyBoxParams const &params)
{ 
  using namespace Vulkan;

  assert(context.ready);
  assert(target->ready());

  wait_fence(context.vulkan, context.fence);

  SkyboxSet skyboxset;
  skyboxset.skycolor = params.skycolor;
  skyboxset.groundcolor = params.groundcolor;
  skyboxset.sundirection = params.sundirection;
  skyboxset.sunintensity = params.sunintensity;
  skyboxset.exposure = params.exposure;
  skyboxset.cloudlayers = 0;

  if (params.clouds)
  {
    assert(params.clouds->ready());

    bind_texture(context.vulkan, context.skyboxdescriptor, 2, params.clouds->albedomap->texture);
    bind_texture(context.vulkan, context.skyboxdescriptor, 3, params.clouds->normalmap->texture);

    skyboxset.cloudlayers = 1;
    skyboxset.cloudheight = params.cloudheight;
    skyboxset.cloudcolor = params.clouds->color;
  }

  auto &commandbuffer = context.commandbuffer;

  bind_image(context.vulkan, context.skyboxdescriptor, 1, target->texture);

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  bind_pipeline(commandbuffer, context.skyboxpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  bind_descriptor(commandbuffer, context.pipelinelayout, 0, context.skyboxdescriptor, VK_PIPELINE_BIND_POINT_COMPUTE);

  push(commandbuffer, context.pipelinelayout, 0, sizeof(skyboxset), &skyboxset, VK_SHADER_STAGE_COMPUTE_BIT);

  setimagelayout(commandbuffer, target->texture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, target->texture.levels, 0, target->texture.layers });

  dispatch(commandbuffer, target->texture.width/16, target->texture.height/16, 1);

  bind_pipeline(commandbuffer, context.convolvepipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  if (params.convolesamples != 0)
  {
    uint32_t levels = 8;

    for(uint32_t level = 1; level < levels; ++level)
    {
      ConvolveSet convolveset;
      convolveset.level = level;
      convolveset.samples = params.convolesamples;
      convolveset.roughness = (float)level / (float)(levels - 1);

      VkImageViewCreateInfo viewinfo = {};
      viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewinfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
      viewinfo.format = target->texture.format;
      viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 6 };
      viewinfo.image = target->texture.image;

      context.convolveimageviews[level] = create_imageview(context.vulkan, viewinfo);

      bind_texture(context.vulkan, context.convolvedescriptors[level], 0, target->texture.imageview, target->texture.sampler, VK_IMAGE_LAYOUT_GENERAL);

      bind_image(context.vulkan, context.convolvedescriptors[level], 1, context.convolveimageviews[level], VK_IMAGE_LAYOUT_GENERAL);

      bind_descriptor(commandbuffer, context.pipelinelayout, 0, context.convolvedescriptors[level], VK_PIPELINE_BIND_POINT_COMPUTE);

      push(commandbuffer, context.pipelinelayout, 0, sizeof(convolveset), &convolveset, VK_SHADER_STAGE_COMPUTE_BIT);

      dispatch(commandbuffer, (target->texture.width + 15)/16, (target->texture.height + 15)/16, 1);
    }
  }
  else
  {
    mip(commandbuffer, target->texture.image, target->texture.width, target->texture.height, target->texture.layers, target->texture.levels);
  }

  setimagelayout(commandbuffer, target->texture.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, target->texture.levels, 0, target->texture.layers });

  end(context.vulkan, commandbuffer);

  //
  // Submit
  //

  submit(context.vulkan, commandbuffer, context.rendercomplete, context.fence);

  if (params.wait)
  {
    while (!test_fence(context.vulkan, context.fence))
      ;
  }
}
