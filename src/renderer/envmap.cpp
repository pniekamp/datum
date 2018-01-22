//
// Datum - envmap
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "envmap.h"
#include "assetpack.h"
#include "renderer.h"
#include <numeric>
#include "debug.h"

using namespace std;
using namespace lml;
using namespace Vulkan;
using leap::alignto;
using leap::extentof;

enum ShaderLocation
{
  convolvesrc = 0,
  convolvedst = 1,

  projectsrc = 0,
  projectdst = 1,
};

struct ConvolveSet
{
  alignas( 4) uint32_t level;
  alignas( 4) uint32_t samples;
  alignas( 4) float roughness;
};

struct ProjectSet
{
  alignas( 4) float a;
};

namespace
{
  size_t envmap_datasize(int width, int height, int layers, int levels, VkFormat format)
  {
    assert(width > 0 && height > 0 && layers > 0 && levels > 0);

    size_t size = 0;
    for(int i = 0; i < levels; ++i)
    {
      switch(format)
      {
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


//|---------------------- EnvMap --------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
EnvMap const *ResourceManager::create<EnvMap>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  assert(asset->layers == 6);
  assert(asset->format == PackImageHeader::rgbe);

  auto slot = acquire_slot(sizeof(EnvMap));

  if (!slot)
    return nullptr;

  auto envmap = new(slot) EnvMap;

  envmap->width = asset->width;
  envmap->height = asset->height;
  envmap->format = EnvMap::Format::RGBE;
  envmap->asset = asset;
  envmap->transferlump = nullptr;
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
  envmap->transferlump = nullptr;
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

  auto setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
  auto setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  auto setupfence = create_fence(vulkan, 0);

  begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  envmap->texture = create_texture(vulkan, setupbuffer, width, height, 6, 8, vkformat, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

  end(vulkan, setupbuffer);

  submit(setupbuffer, setupfence);

  wait_fence(vulkan, setupfence);

  envmap->state = EnvMap::State::Ready;

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

  wait_fence(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  update_texture(lump->commandbuffer, lump->transferbuffer, 0, slot->texture);

  end(vulkan, lump->commandbuffer);

  submit(lump);

  while (!test_fence(vulkan, lump->fence))
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
      assert(assets()->barriercount != 0);

      if (auto bits = m_assets->request(platform, asset))
      {
        VkFormat vkformat = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;

        if (auto lump = acquire_lump(envmap_datasize(asset->width, asset->height, asset->layers, asset->levels, vkformat)))
        {
          wait_fence(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          slot->texture = create_texture(vulkan, lump->commandbuffer, asset->width, asset->height, asset->layers, asset->levels, vkformat, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

          memcpy(lump->memory(), bits, lump->transferbuffer.size);

          update_texture(lump->commandbuffer, lump->transferbuffer, 0, slot->texture);

          end(vulkan, lump->commandbuffer);

          submit(lump);

          slot->transferlump = lump;
        }
      }
    }

    slot->state = (slot->texture) ? EnvMap::State::Waiting : EnvMap::State::Empty;
  }

  EnvMap::State waiting = EnvMap::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, EnvMap::State::Testing))
  {
    bool ready = false;

    if (test_fence(vulkan, slot->transferlump->fence))
    {
      release_lump(slot->transferlump);

      slot->transferlump = nullptr;

      ready = true;
    }

    slot->state = (ready) ? EnvMap::State::Ready : EnvMap::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<EnvMap>(EnvMap const *envmap)
{
  defer_destroy(envmap);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<EnvMap>(EnvMap const *envmap)
{
  if (envmap)
  {
    if (envmap->transferlump)
      release_lump(envmap->transferlump);

    envmap->~EnvMap();

    release_slot(const_cast<EnvMap*>(envmap), sizeof(EnvMap));
  }
}


//|---------------------- Convolve ------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// initialise_convolve_context ///////////////////////
void initialise_convolve_context(DatumPlatform::PlatformInterface &platform, ConvolveContext &context, uint32_t queueindex)
{
  //
  // Vulkan Device
  //

  auto renderdevice = platform.render_device();

  initialise_vulkan_device(&context.vulkan, renderdevice.physicaldevice, renderdevice.device, renderdevice.queues[queueindex].queue, renderdevice.queues[queueindex].familyindex);

  context.commandpool = create_commandpool(context.vulkan, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  context.commandbuffer = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  context.fence = create_fence(context.vulkan);
}


///////////////////////// prepare_convolve_context //////////////////////////
bool prepare_convolve_context(DatumPlatform::PlatformInterface &platform, ConvolveContext &context, AssetManager &assets)
{
  if (context.ready)
    return true;

  assert(context.vulkan);

  if (context.descriptorpool == 0)
  {
    // DescriptorPool

    VkDescriptorPoolSize typecounts[2] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[0].descriptorCount = 7;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    typecounts[1].descriptorCount = 7;

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
    // Convolve Set

    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;

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
    constants[0].size = sizeof(ConvolveSet);

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


///////////////////////// convolve //////////////////////////////////////////
void convolve(ConvolveContext &context, EnvMap const *target, ConvolveParams const &params, VkSemaphore const (&dependancies)[8])
{
  assert(context.ready);
  assert(target->ready());

  uint32_t levels = 8;

  auto &commandbuffer = context.commandbuffer;

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  bind_pipeline(commandbuffer, context.convolvepipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  for(uint32_t level = 0; level < levels; ++level)
  {
    VkImageViewCreateInfo viewinfo = {};
    viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewinfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewinfo.format = target->texture.format;
    viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 6 };
    viewinfo.image = target->texture.image;

    context.convolveimageviews[level] = create_imageview(context.vulkan, viewinfo);
  }

  for(uint32_t level = 1; level < levels; ++level)
  {
    ConvolveSet convolveset;
    convolveset.level = level;
    convolveset.samples = params.samples;
    convolveset.roughness = (float)level / (float)(levels - 1);

    bind_texture(context.vulkan, context.convolvedescriptors[level], ShaderLocation::convolvesrc, context.convolveimageviews[level-1], target->texture.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bind_image(context.vulkan, context.convolvedescriptors[level], ShaderLocation::convolvedst, context.convolveimageviews[level], VK_IMAGE_LAYOUT_GENERAL);

    bind_descriptor(commandbuffer, context.pipelinelayout, 0, context.convolvedescriptors[level], VK_PIPELINE_BIND_POINT_COMPUTE);

    push(commandbuffer, context.pipelinelayout, 0, sizeof(ConvolveSet), &convolveset, VK_SHADER_STAGE_COMPUTE_BIT);

    setimagelayout(commandbuffer, target->texture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 6 });

    dispatch(commandbuffer, target->texture.width, target->texture.height, 1, { 16, 16, 1 });

    setimagelayout(commandbuffer, target->texture.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 6 });
  }

  end(context.vulkan, commandbuffer);

  //
  // Submit
  //

  submit(context.vulkan, commandbuffer, context.fence, dependancies);

  wait_fence(context.vulkan, context.fence);
}


//|---------------------- Project -------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// initialise_project_context ////////////////////////
void initialise_project_context(DatumPlatform::PlatformInterface &platform, ProjectContext &context, uint32_t queueindex)
{
  //
  // Vulkan Device
  //

  auto renderdevice = platform.render_device();

  initialise_vulkan_device(&context.vulkan, renderdevice.physicaldevice, renderdevice.device, renderdevice.queues[queueindex].queue, renderdevice.queues[queueindex].familyindex);

  context.commandpool = create_commandpool(context.vulkan, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  context.commandbuffer = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  context.fence = create_fence(context.vulkan);
}


///////////////////////// prepare_project_context ///////////////////////////
bool prepare_project_context(DatumPlatform::PlatformInterface &platform, ProjectContext &context, AssetManager &assets)
{
  if (context.ready)
    return true;

  assert(context.vulkan);

  if (context.descriptorpool == 0)
  {
    // DescriptorPool

    VkDescriptorPoolSize typecounts[2] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[0].descriptorCount = 1;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    typecounts[1].descriptorCount = 1;

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
    // Project Set

    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;

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
    constants[0].size = sizeof(ProjectSet);

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

  if (context.projectpipeline == 0)
  {
    auto cs = assets.find(CoreAsset::project_comp);

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

    context.projectpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);

    context.projectdescriptor = allocate_descriptorset(context.vulkan, context.descriptorpool, context.descriptorsetlayout);
  }

  if (context.probebuffer == 0)
  {
    context.probebuffer = create_transferbuffer(context.vulkan, sizeof(Irradiance));
  }

  context.ready = true;

  return true;
}


///////////////////////// project ///////////////////////////////////////////
void project(ProjectContext &context, EnvMap const *source, Irradiance &target, ProjectParams const &params, VkSemaphore const (&dependancies)[8])
{
  assert(context.ready);
  assert(source->ready());

  auto &commandbuffer = context.commandbuffer;

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  bind_pipeline(commandbuffer, context.projectpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  bind_texture(context.vulkan, context.projectdescriptor, ShaderLocation::projectsrc, source->texture);

  bind_buffer(context.vulkan, context.projectdescriptor, ShaderLocation::projectdst, context.probebuffer, 0, context.probebuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  bind_descriptor(commandbuffer, context.pipelinelayout, 0, context.projectdescriptor, VK_PIPELINE_BIND_POINT_COMPUTE);

  //push(commandbuffer, context.pipelinelayout, 0, sizeof(ProjectSet), &projectset, VK_SHADER_STAGE_COMPUTE_BIT);

  dispatch(commandbuffer, 1, 1, 1);

  barrier(commandbuffer, context.probebuffer, 0, context.probebuffer.size);

  end(context.vulkan, commandbuffer);

  //
  // Submit
  //

  submit(context.vulkan, commandbuffer, context.fence, dependancies);

  wait_fence(context.vulkan, context.fence);

  target = *map_memory<Irradiance>(context.vulkan, context.probebuffer, 0, sizeof(Irradiance));
}
