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
  extendedset = 3,

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

struct FoilageSet
{
  alignas(16) Vec4 wind;
  alignas(16) Vec3 bendscale;
  alignas(16) Vec3 detailbendscale;
  alignas(16) Transform modelworlds[1];
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

  if (state.pipeline != context.modelspotmappipeline)
  {
    bind_pipeline(castercommands, context.modelspotmappipeline, 0, 0, state.width, state.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.modelspotmappipeline;
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

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.rendercontext->whitediffuse);

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

  if (state.pipeline != context.actorspotmappipeline)
  {
    bind_pipeline(castercommands, context.actorspotmappipeline, 0, 0, state.width, state.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.actorspotmappipeline;
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

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.rendercontext->whitediffuse);

      bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  size_t actorsetsize = sizeof(ActorSet) + (pose.bonecount-1)*sizeof(Transform);

  if (state.modelset.available() < actorsetsize)
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, actorsetsize, std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(actorsetsize);

    auto modelset = state.modelset.memory<ActorSet>(offset);

    modelset->modelworld = transform;

    copy(pose.bones, pose.bones + pose.bonecount, modelset->bones);

    bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(castercommands, mesh->vertexbuffer.indexcount, 1, 0, 0, 0);
  }
}


///////////////////////// SpotCasterList::push_foilage //////////////////////
void SpotCasterList::push_foilage(BuildState &state, Transform const *transforms, size_t count, Mesh const *mesh, Material const *material, Vec4 const &wind, Vec3 const &bendscale, Vec3 const &detailbendscale)
{
  assert(state.commandlump);
  assert(mesh && mesh->ready());
  assert(material && material->ready());

  auto &context = *state.context;
  auto &commandlump = *state.commandlump;

  if (state.pipeline != context.foilagespotmappipeline)
  {
    bind_pipeline(castercommands, context.foilagespotmappipeline, 0, 0, state.width, state.height, VK_PIPELINE_BIND_POINT_GRAPHICS);

    state.pipeline = context.foilagespotmappipeline;
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

      bind_texture(context.vulkan, state.materialset, ShaderLocation::albedomap, material->albedomap ? material->albedomap->texture : context.rendercontext->whitediffuse);

      bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::materialset, state.materialset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

      state.material = material;
    }
  }

  size_t foilagesetsize = sizeof(FoilageSet) + (count-1)*sizeof(Transform);

  if (state.modelset.available() < foilagesetsize)
  {
    state.modelset = commandlump.acquire_descriptor(context.modelsetlayout, foilagesetsize, std::move(state.modelset));
  }

  if (state.modelset && state.materialset)
  {
    auto offset = state.modelset.reserve(foilagesetsize);

    auto modelset = state.modelset.memory<FoilageSet>(offset);

    modelset->wind = wind;
    modelset->bendscale = bendscale;
    modelset->detailbendscale = detailbendscale;

    for(size_t i = 0; i < count; ++i)
    {
      modelset->modelworlds[i] = transforms[i];
    }

    bind_descriptor(castercommands, context.pipelinelayout, ShaderLocation::modelset, state.modelset, offset, VK_PIPELINE_BIND_POINT_GRAPHICS);

    draw(castercommands, mesh->vertexbuffer.indexcount, count, 0, 0, 0);
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
SpotMap const *ResourceManager::create<SpotMap>(int width, int height)
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

  spotmap->texture = create_texture(vulkan, 0, width, height, 1, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

  VkSamplerCreateInfo depthsamplerinfo = {};
  depthsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  depthsamplerinfo.magFilter = VK_FILTER_LINEAR;
  depthsamplerinfo.minFilter = VK_FILTER_LINEAR;
  depthsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  depthsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  depthsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  depthsamplerinfo.compareEnable = VK_TRUE;
  depthsamplerinfo.compareOp = VK_COMPARE_OP_LESS;

  spotmap->texture.sampler = create_sampler(vulkan, depthsamplerinfo);

  spotmap->state = SpotMap::State::Ready;

  return spotmap;
}

template<>
SpotMap const *ResourceManager::create<SpotMap>(uint32_t width, uint32_t height)
{
  return create<SpotMap>((int)width, (int)height);
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

          slot->texture = create_texture(vulkan, lump->commandbuffer, asset->width, asset->height, 1, 1, VK_FORMAT_R32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

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

  context.fence = create_fence(context.vulkan, VK_FENCE_CREATE_SIGNALED_BIT);

  context.rendercomplete = create_semaphore(context.vulkan);
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
  context.materialsetlayout = rendercontext.materialsetlayout;
  context.modelsetlayout = rendercontext.modelsetlayout;

  if (context.descriptorpool == 0)
  {
    // DescriptorPool

    VkDescriptorPoolSize typecounts[2] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    typecounts[0].descriptorCount = 16;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[1].descriptorCount = 48;

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

  if (context.renderpass == 0)
  {
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

    context.srcblitpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);

    for(size_t i = 0; i < extentof(context.srcblitcommands); ++i)
    {
      context.srcblitcommands[i] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    }

    for(size_t i = 0; i < extentof(context.srcblitdescriptors); ++i)
    {
      context.srcblitdescriptors[i] = allocate_descriptorset(context.vulkan, context.descriptorpool, context.materialsetlayout);
    }

    VkSamplerCreateInfo samplerinfo = {};
    samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerinfo.magFilter = VK_FILTER_LINEAR;
    samplerinfo.minFilter = VK_FILTER_LINEAR;
    samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    context.srcblitsampler = create_sampler(context.vulkan, samplerinfo);
  }

  if (context.modelspotmappipeline == 0)
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
    rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

    context.modelspotmappipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.actorspotmappipeline == 0)
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
    rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

    context.actorspotmappipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.foilagespotmappipeline == 0)
  {
    //
    // Foilage Shadow Pipeline
    //

    auto vs = assets.find(CoreAsset::foilage_spotmap_vert);
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
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

    context.foilagespotmappipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  context.rendercontext = &rendercontext;

  context.ready = true;

  return true;
}


///////////////////////// prepare_framebuffer ///////////////////////////////
void prepare_framebuffer(SpotMapContext &context, SpotMap const *target)
{
  if (target->framebuffer == 0)
  {
    VkFramebufferCreateInfo framebufferinfo = {};
    framebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferinfo.renderPass = context.renderpass;
    framebufferinfo.attachmentCount = 1;
    framebufferinfo.pAttachments = target->texture.imageview.data();
    framebufferinfo.width = target->width;
    framebufferinfo.height = target->height;
    framebufferinfo.layers = 1;

    target->framebuffer = create_framebuffer(context.vulkan, framebufferinfo);

    setimagelayout(context.commandbuffer, target->texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
}


///////////////////////// render ////////////////////////////////////////////
void render_spotmaps(SpotMapContext &context, SpotMapInfo const *spotmaps, size_t spotmapcount, SpotMapParams const &params, VkSemaphore const (&dependancies)[8])
{
  assert(context.ready);
  assert(spotmapcount < extentof(context.srcblitcommands));
  assert(spotmapcount < extentof(context.srcblitdescriptors));

  wait_fence(context.vulkan, context.fence);

  VkClearValue clearvalues[1];
  clearvalues[0].depthStencil = { 1, 0 };

  auto &commandbuffer = context.commandbuffer;

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  for(size_t i = 0; i < spotmapcount; ++i)
  {
    auto &target = spotmaps[i].target;
    auto &source = spotmaps[i].source;
    auto &casters = spotmaps[i].casters;

    assert(target->ready() && target->asset == nullptr);

    SceneSet sceneset;
    sceneset.view = inverse(spotmaps[i].spotview).matrix();

    prepare_framebuffer(context, target);

    push(context.commandbuffer, context.pipelinelayout, 0, sizeof(SceneSet), &sceneset, VK_SHADER_STAGE_VERTEX_BIT);

    beginpass(commandbuffer, context.renderpass, target->framebuffer, 0, 0, target->width, target->height, 1, &clearvalues[0]);

    if (source)
    {
      assert(source->ready());

      auto &srcblitcommands = context.srcblitcommands[i];
      auto &srcblitdescriptor = context.srcblitdescriptors[i];

      begin(context.vulkan, srcblitcommands, target->framebuffer, context.renderpass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
      bind_pipeline(srcblitcommands, context.srcblitpipeline, 0, 0, target->width, target->height, VK_PIPELINE_BIND_POINT_GRAPHICS);
      bind_texture(context.vulkan, srcblitdescriptor, ShaderLocation::sourcemap, source->texture.imageview, context.srcblitsampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      bind_descriptor(srcblitcommands, context.pipelinelayout, ShaderLocation::materialset, srcblitdescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);
      bind_vertexbuffer(srcblitcommands, 0, context.rendercontext->unitquad);
      draw(srcblitcommands, context.rendercontext->unitquad.vertexcount, 1, 0, 0);
      end(context.vulkan, srcblitcommands);

      execute(commandbuffer, srcblitcommands);
    }

    if (casters->castercommands)
    {
      execute(commandbuffer, casters->castercommands);
    }

    endpass(commandbuffer, context.renderpass);
  }

  end(context.vulkan, commandbuffer);

  //
  // Submit
  //

  submit(context.vulkan, context.commandbuffer, context.rendercomplete, context.fence, dependancies);
}
