//
// Datum - skybox
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "skybox.h"
#include "renderer.h"
#include <leap/lml/matrixconstants.h>
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

  begin(context.device, skyboxcommands, context.forwardbuffer, context.forwardpass, RenderPasses::skyboxpass, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bindresource(skyboxcommands, context.skyboxpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

  bindtexture(context.device, skyboxdescriptor, ShaderLocation::skyboxmap, params.skybox->envmap->texture);

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

  skybox->envmap = create<EnvMap>(asset);

  return skybox;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
SkyBox const *ResourceManager::create<SkyBox>(int width, int height, EnvMap::Format format)
{
  auto slot = acquire_slot(sizeof(SkyBox));

  if (!slot)
    return nullptr;

  auto skybox = new(slot) SkyBox;

  skybox->envmap = create<EnvMap>(width, height, format);

  return skybox;
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
  assert(lump);
  assert(skybox);
  assert(skybox->envmap);

  update<EnvMap>(skybox->envmap, lump);
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<SkyBox>(DatumPlatform::PlatformInterface &platform, SkyBox const *skybox)
{
  assert(skybox);
  assert(skybox->envmap);

  request(platform, skybox->envmap);
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

  destroy(skybox->envmap);

  skybox->~SkyBox();

  release_slot(const_cast<SkyBox*>(skybox), sizeof(SkyBox));
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

  bindimageview(context.device, context.descriptorset, 0, skybox->envmap->texture.imageview);

  begin(context.device, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  bindresource(commandbuffer, context.pipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  bindresource(commandbuffer, context.descriptorset, context.pipelinelayout, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

  push(commandbuffer, context.pipelinelayout, 0, sizeof(skyboxset), &skyboxset, VK_SHADER_STAGE_COMPUTE_BIT);

  dispatch(commandbuffer, (skybox->envmap->texture.width + 15)/16, (skybox->envmap->texture.height + 15)/16, 1);

  mip(commandbuffer, skybox->envmap->texture.image, skybox->envmap->texture.width, skybox->envmap->texture.height, skybox->envmap->texture.layers, skybox->envmap->texture.levels);

  end(context.device, commandbuffer);

  //
  // Submit
  //

  submit(context.device, commandbuffer, context.fence);

  wait(context.device, context.fence);
}

