//
// Datum - spotmap
//

//
// Copyright (c) 2017 Peter Niekamp
//

#include "spotmap.h"
#include "renderer.h"
#include "assetpack.h"
#include <leap/lml/matrixconstants.h>
#include <numeric>
#include "debug.h"

using namespace std;
using namespace lml;
using namespace Vulkan;
using leap::alignto;
using leap::extentof;

enum VertexLayout
{
  position_offset = 0,
  texcoord_offset = 12,
  normal_offset = 20,
  tangent_offset = 32,

  stride = 48
};

enum ShaderLocation
{
  // Bindings

  vertex_position = 0,
  vertex_texcoord = 1,
  vertex_normal = 2,
  vertex_tangent = 3,

  rig_bone = 4,
  rig_weight = 5,

  sceneset = 0,
  materialset = 1,
  modelset = 2,

  scenebuf = 0,
  sourcemap = 1,

  albedomap = 1,
};

struct SceneSet
{
  alignas(16) Matrix4f view;
};

struct MaterialSet
{
};

struct ModelSet
{
  alignas(16) Transform modelworld;
};

struct ActorSet
{
  alignas(16) Transform modelworld;
  alignas(16) Transform bones[1];
};

namespace
{
  size_t spotmap_datasize(int width, int height)
  {
    return width * height * sizeof(uint32_t);
  }
}

//|---------------------- SpotCasterList ------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// SpotCasterList::begin /////////////////////////////
bool SpotCasterList::begin(BuildState &state, SpotMapContext &context, ResourceManager &resources)
{
  m_commandlump = {};

  state = {};
  state.context = &context;
  state.resources = &resources;

  if (!context.ready)
    return false;

  auto commandlump = resources.allocate<CommandLump>(context.rendercontext);

  if (!commandlump)
    return false;

  castercommands = commandlump->allocate_commandbuffer();

  if (!castercommands)
  {
    resources.destroy(commandlump);
    return false;
  }

  using Vulkan::begin;

  begin(context.vulkan, castercommands, 0, context.renderpass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::sceneset, context.scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);

  m_commandlump = { resources, commandlump };

  state.commandlump = commandlump;

  return true;
}


///////////////////////// SpotCasterList::push_mesh /////////////////////////
void SpotCasterList::push_mesh(BuildState &state, Transform const &transform, Mesh const *mesh, Material const *material)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.pipeline != context.modelshadowpipeline)
  {
    bind_pipeline(castercommands, context.modelshadowpipeline, 0, 0, state.width, state.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.modelshadowpipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(castercommands, 0, mesh->vertexbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(MaterialSet));

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

      bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  if (state.modelset.available() < sizeof(ModelSet))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(ModelSet), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ModelSet));

    auto modelset = state.modelset.memory<ModelSet>(offset);

    modelset->modelworld = transform;

    bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(castercommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// SpotCasterList::push_mesh /////////////////////////
void SpotCasterList::push_mesh(BuildState &state, Transform const &transform, Pose const &pose, Mesh const *mesh, Material const *material)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.pipeline != context.actorshadowpipeline)
  {
    bind_pipeline(castercommands, context.actorshadowpipeline, 0, 0, state.width, state.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.actorshadowpipeline;
  }

  if (state.mesh != mesh)
  {
    bind_vertexbuffer(castercommands, 0, mesh->vertexbuffer);
    bind_vertexbuffer(castercommands, 1, mesh->rigbuffer);

    state.mesh = mesh;
  }

  if (state.material != material)
  {
    state.materialset = commandlump.acquire_descriptor(context.materialsetlayout, sizeof(MaterialSet), std::move(state.materialset));

    if (state.materialset)
    {
      auto offset = state.materialset.reserve(sizeof(MaterialSet));

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.whitediffuse);

      bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  if (state.modelset.available() < sizeof(ActorSet) + pose.bonecount*sizeof(Transform))
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, sizeof(ActorSet) + pose.bonecount*sizeof(Transform), std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(sizeof(ActorSet) + pose.bonecount*sizeof(Transform));

    auto modelset = state.modelset.memory<ActorSet>(offset);

    modelset->modelworld = transform;

    copy(pose.bones, pose.bones + pose.bonecount, modelset->bones);

    bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(castercommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// SpotCasterList::finalise //////////////////////////
void SpotCasterList::finalise(BuildState &state)
{
  assert(state.commandlump);

  auto &context = *state.context;

  end(context.vulkan, castercommands);

  state.commandlump = nullptr;
}


//|---------------------- SpotMap -------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
SpotMap const *ResourceManager::create<SpotMap>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(SpotMap));

  if (!slot)
    return nullptr;

  auto spotmap = new(slot) SpotMap;

  spotmap->width = asset->width;
  spotmap->height = asset->height;
  spotmap->asset = asset;
  spotmap->transferlump = nullptr;
  spotmap->state = SpotMap::State::Empty;

  return spotmap;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
SpotMap const *ResourceManager::create<SpotMap>(SpotMapContext *context, int width, int height)
{
  auto slot = acquire_slot(sizeof(SpotMap));

  if (!slot)
    return nullptr;

  auto spotmap = new(slot) SpotMap;

  spotmap->width = width;
  spotmap->height = height;
  spotmap->asset = nullptr;
  spotmap->transferlump = nullptr;
  spotmap->state = SpotMap::State::Empty;

  auto setuppool = create_commandpool(context->vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
  auto setupbuffer = allocate_commandbuffer(context->vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  auto setupfence = create_fence(context->vulkan, 0);

  begin(context->vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  spotmap->texture = create_texture(context->vulkan, setupbuffer, width, height, 1, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  end(context->vulkan, setupbuffer);

  {
    leap::threadlib::SyncLock lock(context->m_mutex);

    Vulkan::submit(context->vulkan, setupbuffer, setupfence);
  }

  wait_fence(context->vulkan, setupfence);

  VkSamplerCreateInfo depthsamplerinfo = {};
  depthsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  depthsamplerinfo.magFilter = VK_FILTER_LINEAR;
  depthsamplerinfo.minFilter = VK_FILTER_LINEAR;
  depthsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  depthsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  depthsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  depthsamplerinfo.compareEnable = VK_TRUE;
  depthsamplerinfo.compareOp = VK_COMPARE_OP_LESS;

  spotmap->texture.sampler = create_sampler(context->vulkan, depthsamplerinfo);

  VkFramebufferCreateInfo framebufferinfo = {};
  framebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferinfo.renderPass = context->renderpass;
  framebufferinfo.attachmentCount = 1;
  framebufferinfo.pAttachments = spotmap->texture.imageview.data();
  framebufferinfo.width = width;
  framebufferinfo.height = height;
  framebufferinfo.layers = 1;

  spotmap->framebuffer = create_framebuffer(context->vulkan, framebufferinfo);

  spotmap->state = SpotMap::State::Ready;

  return spotmap;
}

template<>
SpotMap const *ResourceManager::create<SpotMap>(SpotMapContext *context, uint32_t width, uint32_t height)
{
  return create<SpotMap>(context, (int)width, (int)height);
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<SpotMap>(DatumPlatform::PlatformInterface &platform, SpotMap const *spotmap)
{
  assert(spotmap);

  auto slot = const_cast<SpotMap*>(spotmap);

  SpotMap::State empty = SpotMap::State::Empty;

  if (slot->state.compare_exchange_strong(empty, SpotMap::State::Loading))
  {
    if (auto asset = slot->asset)
    {
      assert(assets()->barriercount != 0);

      if (auto bits = m_assets->request(platform, asset))
      {
        assert(asset->format == PackImageHeader::depth);

        if (auto lump = acquire_lump(spotmap_datasize(asset->width, asset->height)))
        {
          wait_fence(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          slot->texture = create_texture(vulkan, lump->commandbuffer, asset->width, asset->height, 1, 1, VK_FORMAT_R32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

          memcpy(lump->memory(), bits, lump->transferbuffer.size);

          update_texture(lump->commandbuffer, lump->transferbuffer, 0, slot->texture);

          end(vulkan, lump->commandbuffer);

          submit(lump);

          slot->transferlump = lump;

          VkSamplerCreateInfo depthsamplerinfo = {};
          depthsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
          depthsamplerinfo.magFilter = VK_FILTER_LINEAR;
          depthsamplerinfo.minFilter = VK_FILTER_LINEAR;
          depthsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
          depthsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
          depthsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
          depthsamplerinfo.compareEnable = VK_TRUE;
          depthsamplerinfo.compareOp = VK_COMPARE_OP_LESS;

          slot->texture.sampler = create_sampler(vulkan, depthsamplerinfo);
        }
      }
    }

    slot->state = (slot->texture) ? SpotMap::State::Waiting : SpotMap::State::Empty;
  }

  SpotMap::State waiting = SpotMap::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, SpotMap::State::Testing))
  {
    bool ready = false;

    if (test_fence(vulkan, slot->transferlump->fence))
    {
      release_lump(slot->transferlump);

      slot->transferlump = nullptr;

      ready = true;
    }

    slot->state = (ready) ? SpotMap::State::Ready : SpotMap::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<SpotMap>(SpotMap const *spotmap)
{
  defer_destroy(spotmap);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<SpotMap>(SpotMap const *spotmap)
{
  if (spotmap)
  {
    if (spotmap->transferlump)
      release_lump(spotmap->transferlump);

    spotmap->~SpotMap();

    release_slot(const_cast<SpotMap*>(spotmap), sizeof(SpotMap));
  }
}


//|---------------------- SpotMapRenderer -----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// initialise_spotmap_context ////////////////////////
void initialise_spotmap_context(DatumPlatform::PlatformInterface &platform, SpotMapContext &context, uint32_t queueindex)
{
  //
  // Vulkan Device
  //

  auto renderdevice = platform.render_device();

  initialise_vulkan_device(&context.vulkan, renderdevice.physicaldevice, renderdevice.device, renderdevice.queues[queueindex].queue, renderdevice.queues[queueindex].familyindex);

  context.commandpool = create_commandpool(context.vulkan, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  context.commandbuffer = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  context.fence = create_fence(context.vulkan);

  //
  // Render Pass
  //

  VkAttachmentDescription attachments[1] = {};
  attachments[0].format = VK_FORMAT_D32_SFLOAT;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference depthreference = {};
  depthreference.attachment = 0;
  depthreference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpasses[1] = {};
  subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpasses[0].pDepthStencilAttachment = &depthreference;

  VkRenderPassCreateInfo renderpassinfo = {};
  renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderpassinfo.attachmentCount = extentof(attachments);
  renderpassinfo.pAttachments = attachments;
  renderpassinfo.subpassCount = extentof(subpasses);
  renderpassinfo.pSubpasses = subpasses;

  context.renderpass = create_renderpass(context.vulkan, renderpassinfo);
}


///////////////////////// prepare_spotmap_context ///////////////////////////
bool prepare_spotmap_context(DatumPlatform::PlatformInterface &platform, SpotMapContext &context, RenderContext &rendercontext, AssetManager &assets)
{
  if (context.ready)
    return true;

  assert(context.vulkan);

  if (!rendercontext.ready)
    return false;

  context.pipelinelayout = rendercontext.pipelinelayout;
  context.scenesetlayout = rendercontext.scenesetlayout;
  context.materialsetlayout = rendercontext.materialsetlayout;
  context.modelsetlayout = rendercontext.modelsetlayout;

  if (context.descriptorpool == 0)
  {
    // DescriptorPool

    VkDescriptorPoolSize typecounts[2] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    typecounts[0].descriptorCount = 2;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[1].descriptorCount = 12;

    VkDescriptorPoolCreateInfo descriptorpoolinfo = {};
    descriptorpoolinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorpoolinfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorpoolinfo.maxSets = accumulate(begin(typecounts), end(typecounts), 0, [](int i, auto &k) { return i + k.descriptorCount; });
    descriptorpoolinfo.poolSizeCount = extentof(typecounts);
    descriptorpoolinfo.pPoolSizes = typecounts;

    context.descriptorpool = create_descriptorpool(context.vulkan, descriptorpoolinfo);
  }

  if (context.sceneset == 0)
  {
    context.sceneset = create_storagebuffer(context.vulkan, sizeof(SceneSet));

    context.scenedescriptor = allocate_descriptorset(context.vulkan, context.descriptorpool, context.scenesetlayout);
    bind_buffer(context.vulkan, context.scenedescriptor, ShaderLocation::scenebuf, context.sceneset, 0, context.sceneset.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  }

  if (context.srcblitpipeline == 0)
  {
    //
    // Source Blit Pipeline
    //

    auto vs = assets.find(CoreAsset::spotmap_src_vert);
    auto fs = assets.find(CoreAsset::spotmap_src_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(rendercontext.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = rendercontext.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.renderpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.srcblitpipeline = create_pipeline(context.vulkan, rendercontext.pipelinecache, pipelineinfo);

    context.srcblitcommands = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    context.srcblitdescriptor = allocate_descriptorset(context.vulkan, context.descriptorpool, context.scenesetlayout);

    bind_buffer(context.vulkan, context.srcblitdescriptor, ShaderLocation::scenebuf, context.sceneset, 0, context.sceneset.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    VkSamplerCreateInfo samplerinfo = {};
    samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerinfo.magFilter = VK_FILTER_LINEAR;
    samplerinfo.minFilter = VK_FILTER_LINEAR;
    samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    context.srcblitsampler = create_sampler(context.vulkan, samplerinfo);
  }

  if (context.modelshadowpipeline == 0)
  {
    //
    // Model Shadow Pipeline
    //

    auto vs = assets.find(CoreAsset::model_spotmap_vert);
    auto fs = assets.find(CoreAsset::spotmap_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(rendercontext.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = rendercontext.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.renderpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.modelshadowpipeline = create_pipeline(context.vulkan, rendercontext.pipelinecache, pipelineinfo);
  }

  if (context.actorshadowpipeline == 0)
  {
    //
    // Actor Shadow Pipeline
    //

    auto vs = assets.find(CoreAsset::actor_spotmap_vert);
    auto fs = assets.find(CoreAsset::spotmap_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[2] = {};
    vertexbindings[0].binding = 0;
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexbindings[1].binding = 1;
    vertexbindings[1].stride = 32;
    vertexbindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexattributes[6] = {};
    vertexattributes[0] = rendercontext.vertexattributes[0];
    vertexattributes[1] = rendercontext.vertexattributes[1];
    vertexattributes[2] = rendercontext.vertexattributes[2];
    vertexattributes[3] = rendercontext.vertexattributes[3];
    vertexattributes[4].binding = 1;
    vertexattributes[4].location = ShaderLocation::rig_bone;
    vertexattributes[4].format = VK_FORMAT_R32G32B32A32_UINT;
    vertexattributes[4].offset = 0;
    vertexattributes[5].binding = 1;
    vertexattributes[5].location = ShaderLocation::rig_weight;
    vertexattributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertexattributes[5].offset = 16;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(vertexattributes);
    vertexinput.pVertexAttributeDescriptions = vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.renderpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.actorshadowpipeline = create_pipeline(context.vulkan, rendercontext.pipelinecache, pipelineinfo);
  }

  if (context.whitediffuse == 0)
  {
    auto image = assets.find(CoreAsset::white_diffuse);

    if (!image)
      return false;

    asset_guard lock(assets);

    auto bits = assets.request(platform, image);

    if (!bits)
      return false;

    context.whitediffuse = create_texture(context.vulkan, rendercontext.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, bits);
  }

  if (context.unitquad == 0)
  {
    auto mesh = assets.find(CoreAsset::unit_quad);

    if (!mesh)
      return false;

    asset_guard lock(assets);

    auto bits = assets.request(platform, mesh);

    if (!bits)
      return false;

    context.unitquad = create_vertexbuffer(context.vulkan, rendercontext.transferbuffer, bits, mesh->vertexcount, sizeof(PackVertex));
  }

  context.rendercontext = &rendercontext;

  context.ready = true;

  return true;
}


///////////////////////// render ////////////////////////////////////////////
void render_spotmap(SpotMapContext &context, SpotMap const *target, SpotCasterList const &casters, SpotMapParams const &params)
{
  using namespace Vulkan;

  assert(context.ready);
  assert(target->ready());
  assert(target->framebuffer);

  BEGIN_TIMED_BLOCK(SpotMap, Color3(0.1f, 0.4f, 0.1f))

  auto &framebuffer = target->framebuffer;

  auto &commandbuffer = context.commandbuffer;

  VkClearValue clearvalues[1];
  clearvalues[0].depthStencil = { 1, 0 };

  SceneSet sceneset;
  sceneset.view = inverse(params.shadowview).matrix();

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  update(commandbuffer, context.sceneset, 0, sizeof(SceneSet), &sceneset);

  beginpass(commandbuffer, context.renderpass, framebuffer, 0, 0, target->width, target->height, 1, &clearvalues[0]);

  if (params.source)
  {
    assert(params.source->ready());

    begin(context.vulkan, context.srcblitcommands, framebuffer, context.renderpass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
    bind_pipeline(context.srcblitcommands, context.srcblitpipeline, 0, 0, target->width, target->height, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_texture(context.vulkan, context.srcblitdescriptor, ShaderLocation::sourcemap, params.source->texture.imageview, context.srcblitsampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    bind_descriptor(context.srcblitcommands, context.pipelinelayout, ShaderLocation::sceneset, context.srcblitdescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_vertexbuffer(context.srcblitcommands, 0, context.unitquad);
    draw(context.srcblitcommands, context.unitquad.vertexcount, 1, 0, 0);
    end(context.vulkan, context.srcblitcommands);

    execute(commandbuffer, context.srcblitcommands);
  }

  if (casters.castercommands)
  {
    execute(commandbuffer, casters.castercommands);
  }

  endpass(commandbuffer, context.renderpass);

  end(context.vulkan, commandbuffer);

  //
  // Submit
  //

  {
    leap::threadlib::SyncLock lock(context.m_mutex);

    Vulkan::submit(context.vulkan, context.commandbuffer, context.fence);
  }

  wait_fence(context.vulkan, context.fence);

  END_TIMED_BLOCK(SpotMap)
}
