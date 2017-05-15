//
// Datum - renderer
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "renderer.h"
#include "commandlist.h"
#include "lightlist.h"
#include "corepack.h"
#include "assetpack.h"
#include <leap.h>
#include <leap/lml/matrixconstants.h>
#include <vector>
#include <limits>
#include <random>
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
  computeset = 3,

  rt0 = 1,
  rt1 = 2,
  normalmap = 3,
  depthmap = 4,
  shadowmap = 6,
  envbrdf = 7,

  colorsrc = 5,
  ssrsrc = 6,
  bloomsrc = 7,

  lumaset = 0,
  colormap = 1,
  scratchmap0 = 2,
  scratchmap1 = 3,
  scratchmap2 = 4,
  ssaoset = 5,
  ssaomap = 6,
  ssaoprevmap = 7,
  ssaotarget = 8,
  envmaps = 9,
  colortarget = 10,
  scratchtarget0 = 11,
  scratchtarget1 = 12,
  scratchtarget2 = 13,

  // Constant Ids

  SizeX = 1,
  SizeY = 2,
  SizeZ = 3,

  ShadowSlices = 46,
  ShadowRadius = 67,

  MaxPointLights = 29,

  MaxEnvironments = 31,

  MaxTileLights = 72,

  NoiseSize = 62,
  KernelSize = 63,

  SSAORadius = 67,

  BloomCutoff = 81,
  BloomBlurRadius = 84,
  BloomBlurSigma = 85,

  SoftParticles = 28,
};

struct MainLight
{
  alignas(16) Vec3 direction;
  alignas(16) Color3 intensity;
};

struct PointLight
{
  alignas(16) Vec3 position;
  alignas(16) Color3 intensity;
  alignas(16) Vec4 attenuation;
};

struct Environment
{
  alignas(16) Vec3 halfdim;
  alignas(16) Transform invtransform;
};

struct CameraView
{
  alignas(16) Vec3 position;
  alignas(4) float exposure;
  alignas(4) float skyboxlod;
  alignas(4) float ssrstrength;
  alignas(4) float bloomstrength;
};

struct SceneSet
{
  alignas(16) Matrix4f proj;
  alignas(16) Matrix4f invproj;
  alignas(16) Matrix4f view;
  alignas(16) Matrix4f invview;
  alignas(16) Matrix4f worldview;
  alignas(16) Matrix4f prevview;
  alignas(16) Matrix4f skyview;
  alignas(16) Vec4 viewport;

  alignas(16) CameraView camera;

  alignas(16) MainLight mainlight;

  alignas(4) float splits[4];
  alignas(16) Matrix4f shadowview[4];

  alignas(4) uint32_t envcount;
  alignas(16) Environment environments[6];

  alignas(4) uint32_t pointlightcount;
  alignas(16) PointLight pointlights[256];
};

struct SSAOSet
{
  alignas(16) Vec4 noise[16];
  alignas(16) Vec4 kernel[16];
};

struct LumaSet
{
  float luminance;
};

static constexpr size_t TransferBufferSize = 512*1024;

struct ComputeConstants
{
  uint32_t ShadowSlices = ShadowMap::nslices;
  uint32_t MaxPointLights = extent<decltype(SceneSet::pointlights)>::value;

  uint32_t MaxTileLights = 48;
  uint32_t MaxEnvironments = 6;

  uint32_t NoiseSize = extent<decltype(SSAOSet::noise)>::value;
  uint32_t KernelSize = extent<decltype(SSAOSet::kernel)>::value;

  uint32_t SSAORadius = 2;
  uint32_t SSAODispatch[3] = { 28, 28, 1 };
  uint32_t SSAOSizeX = SSAODispatch[0] + SSAORadius + SSAORadius;
  uint32_t SSAOSizeY = SSAODispatch[1] + SSAORadius + SSAORadius;

  uint32_t LightingDispatch[3] = { 16, 16, 1 };
  uint32_t LightingSizeX = LightingDispatch[0];
  uint32_t LightingSizeY = LightingDispatch[1];

  uint32_t SSRDispatch[3] = { 4, 32, 1 };
  uint32_t SSRSizeX = SSRDispatch[0];
  uint32_t SSRSizeY = SSRDispatch[1];

  float BloomCutoff = 7.8f;
  uint32_t BloomLumaDispatch[3] = { 4, 32, 1 };
  uint32_t BloomLumaSizeX = BloomLumaDispatch[0];
  uint32_t BloomLumaSizeY = BloomLumaDispatch[1];

  uint32_t BloomBlurSigma = 8;
  uint32_t BloomBlurRadius = 16;
  uint32_t BloomHBlurDispatch[3] = { 991, 1, 1 };
  uint32_t BloomHBlurSize = BloomHBlurDispatch[0] + BloomBlurRadius + BloomBlurRadius;
  uint32_t BloomVBlurDispatch[3] = { 1, 575, 1 };
  uint32_t BloomVBlurSize = BloomVBlurDispatch[1] + BloomBlurRadius + BloomBlurRadius;

  uint32_t True = true;
  uint32_t False = false;

  uint32_t One = 1;
  uint32_t Two = 2;

} computeconstants;


//|---------------------- PushBuffer ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// PushBuffer::Constructor ///////////////////////////
PushBuffer::PushBuffer(allocator_type const &allocator, size_t slabsize)
{
  m_slabsize = slabsize;
  m_slab = allocate<char>(allocator, m_slabsize, alignof(Header));

  m_tail = m_slab;
}


///////////////////////// PushBuffer::reset /////////////////////////////////
void PushBuffer::reset()
{
  m_tail = m_slab;
}


///////////////////////// PushBuffer::push //////////////////////////////////
void *PushBuffer::push(Renderable::Type type, size_t size, size_t alignment)
{
  assert(((size_t)m_tail & (alignof(Header)-1)) == 0);
  assert(size + alignment + sizeof(Header) + alignof(Header) < std::numeric_limits<decltype(Header::size)>::max());

  auto header = reinterpret_cast<Header*>(m_tail);

  void *aligned = header + 1;

  size_t space = m_slabsize - (reinterpret_cast<size_t>(aligned) - reinterpret_cast<size_t>(m_slab));

  size = ((size - 1)/alignof(Header) + 1) * alignof(Header);

  if (!std::align(alignment, size, aligned, space))
    return nullptr;

  header->type = type;
  header->size = reinterpret_cast<size_t>(aligned) + size - reinterpret_cast<size_t>(header);

  m_tail = static_cast<char*>(aligned) + size;

  return aligned;
}




//|---------------------- Renderer ------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// initialise_render_context //////////////////////////
void initialise_render_context(DatumPlatform::PlatformInterface &platform, RenderContext &context, size_t storagesize, uint32_t queueindex)
{
  //
  // Vulkan Device
  //

  auto renderdevice = platform.render_device();

  initialise_vulkan_device(&context.vulkan, renderdevice.physicaldevice, renderdevice.device, renderdevice.queues[queueindex].queue, renderdevice.queues[queueindex].familyindex);

  // Command Buffers

  context.commandpool = create_commandpool(context.vulkan, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  context.commandbuffers[0] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  context.commandbuffers[1] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  // Transfer Buffer

  context.transferbuffer = create_transferbuffer(context.vulkan, TransferBufferSize);

  context.framefence = create_fence(context.vulkan, VK_FENCE_CREATE_SIGNALED_BIT);

  // Resource Pool

  initialise_resource_pool(platform, context.resourcepool, storagesize, queueindex);

  context.frame = 0;
}


///////////////////////// prepare_render_context ////////////////////////////
bool prepare_render_context(DatumPlatform::PlatformInterface &platform, RenderContext &context, AssetManager *assets)
{
  if (context.ready)
    return true;

  assert(context.vulkan);

  if (context.descriptorpool == 0)
  {
    // DescriptorPool

    VkDescriptorPoolSize typecounts[4] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    typecounts[0].descriptorCount = 16;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    typecounts[1].descriptorCount = 16;
    typecounts[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[2].descriptorCount = 128;
    typecounts[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    typecounts[3].descriptorCount = 16;

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

  if (context.timingquerypool == 0)
  {
    // Timing QueryPool

    VkQueryPoolCreateInfo querypoolinfo = {};
    querypoolinfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    querypoolinfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    querypoolinfo.queryCount = 16;

    context.timingquerypool = create_querypool(context.vulkan, querypoolinfo);
  }

  if (context.pipelinelayout == 0)
  {
    // Vertex Attribute Array

    context.vertexattributes[0] = {};
    context.vertexattributes[0].location = ShaderLocation::vertex_position;
    context.vertexattributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    context.vertexattributes[0].offset = VertexLayout::position_offset;

    context.vertexattributes[1] = {};
    context.vertexattributes[1].location = ShaderLocation::vertex_texcoord;
    context.vertexattributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    context.vertexattributes[1].offset = VertexLayout::texcoord_offset;

    context.vertexattributes[2] = {};
    context.vertexattributes[2].location = ShaderLocation::vertex_normal;
    context.vertexattributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    context.vertexattributes[2].offset = VertexLayout::normal_offset;

    context.vertexattributes[3] = {};
    context.vertexattributes[3].location = ShaderLocation::vertex_tangent;
    context.vertexattributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    context.vertexattributes[3].offset = VertexLayout::tangent_offset;
  }

  if (context.scenesetlayout == 0)
  {
    // Scene Set

    VkDescriptorSetLayoutBinding bindings[8] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[2].binding = 2;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[3].binding = 3;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[4].binding = 4;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[5].binding = 5;
    bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].descriptorCount = 1;
    bindings[6].binding = 6;
    bindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[6].descriptorCount = 1;
    bindings[7].binding = 7;
    bindings[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[7].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.scenesetlayout = create_descriptorsetlayout(context.vulkan, createinfo);
  }

  if (context.materialsetlayout == 0)
  {
    // Material Set

    VkDescriptorSetLayoutBinding bindings[4] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[2].binding = 2;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[3].binding = 3;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.materialsetlayout = create_descriptorsetlayout(context.vulkan, createinfo);
  }

  if (context.modelsetlayout == 0)
  {
    // Model Set

    VkDescriptorSetLayoutBinding bindings[1] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    bindings[0].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.modelsetlayout = create_descriptorsetlayout(context.vulkan, createinfo);
  }

  if (context.computesetlayout == 0)
  {
    // Compute Set

    VkDescriptorSetLayoutBinding bindings[14] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[2].binding = 2;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[3].binding = 3;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[4].binding = 4;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[5].binding = 5;
    bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[6].binding = 6;
    bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[6].descriptorCount = 1;
    bindings[7].binding = 7;
    bindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[7].descriptorCount = 1;
    bindings[8].binding = 8;
    bindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[8].descriptorCount = 1;
    bindings[9].binding = 9;
    bindings[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[9].descriptorCount = 6;
    bindings[10].binding = 10;
    bindings[10].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[10].descriptorCount = 1;
    bindings[11].binding = 11;
    bindings[11].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[11].descriptorCount = 1;
    bindings[12].binding = 12;
    bindings[12].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[12].descriptorCount = 1;
    bindings[13].binding = 13;
    bindings[13].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[13].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.computesetlayout = create_descriptorsetlayout(context.vulkan, createinfo);

    context.framedescriptors[0] = allocate_descriptorset(context.vulkan, context.descriptorpool, context.computesetlayout);
    context.framedescriptors[1] = allocate_descriptorset(context.vulkan, context.descriptorpool, context.computesetlayout);

  }

  if (context.pipelinelayout == 0)
  {
    // PipelineLayout

    VkDescriptorSetLayout layouts[4] = {};
    layouts[0] = context.scenesetlayout;
    layouts[1] = context.materialsetlayout;
    layouts[2] = context.modelsetlayout;
    layouts[3] = context.computesetlayout;

    VkPipelineLayoutCreateInfo pipelinelayoutinfo = {};
    pipelinelayoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelinelayoutinfo.setLayoutCount = extentof(layouts);
    pipelinelayoutinfo.pSetLayouts = layouts;

    context.pipelinelayout = create_pipelinelayout(context.vulkan, pipelinelayoutinfo);
  }

  if (context.shadowpass == 0)
  {
    //
    // Shadow Pass
    //

    VkAttachmentDescription attachments[1] = {};
    attachments[0].format = VK_FORMAT_D32_SFLOAT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

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

    context.shadowpass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.geometrypass == 0)
  {
    //
    // Geometry Pass
    //

    VkAttachmentDescription attachments[4] = {};
    attachments[0].format = VK_FORMAT_B8G8R8A8_SRGB;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].format = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].format = VK_FORMAT_D32_SFLOAT;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorreference[3] = {};
    colorreference[0].attachment = 0;
    colorreference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorreference[1].attachment = 1;
    colorreference[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorreference[2].attachment = 2;
    colorreference[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthreference = {};
    depthreference.attachment = 3;
    depthreference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[1] = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = extentof(colorreference);
    subpasses[0].pColorAttachments = colorreference;
    subpasses[0].pDepthStencilAttachment = &depthreference;

    VkSubpassDependency dependencies[1] = {};
    dependencies[0].srcSubpass = 0;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;
    renderpassinfo.dependencyCount = extentof(dependencies);
    renderpassinfo.pDependencies = dependencies;

    context.geometrypass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.forwardpass == 0)
  {
    //
    // Forward Pass
    //

    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorreference[1] = {};
    colorreference[0].attachment = 0;
    colorreference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthreference = {};
    depthreference.attachment = 1;
    depthreference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[1] = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = extentof(colorreference);
    subpasses[0].pColorAttachments = colorreference;
    subpasses[0].pDepthStencilAttachment = &depthreference;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;

    context.forwardpass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.renderpass == 0)
  {
    //
    // Render Pass
    //

    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = VK_FORMAT_B8G8R8A8_SRGB;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorreference[1] = {};
    colorreference[0].attachment = 0;
    colorreference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthreference = {};
    depthreference.attachment = 1;
    depthreference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[1] = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = extentof(colorreference);
    subpasses[0].pColorAttachments = colorreference;
    subpasses[0].pDepthStencilAttachment = &depthreference;

    VkSubpassDependency dependencies[1] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;
    renderpassinfo.dependencyCount = extentof(dependencies);
    renderpassinfo.pDependencies = dependencies;

    context.renderpass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.sceneset == 0)
  {
    context.sceneset = create_storagebuffer(context.vulkan, sizeof(SceneSet));

    context.scenedescriptor = allocate_descriptorset(context.vulkan, context.descriptorpool, context.scenesetlayout, context.sceneset, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
  }

  if (context.modelpipeline == 0)
  {
    //
    // Model Pipeline
    //

    auto vs = assets->find(CoreAsset::model_vert);
    auto fs = assets->find(CoreAsset::geometry_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[3] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

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
    pipelineinfo.renderPass = context.geometrypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.modelpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.modelshadowpipeline == 0)
  {
    //
    // Model Shadow Pipeline
    //

    auto vs = assets->find(CoreAsset::model_shadow_vert);
    auto gs = assets->find(CoreAsset::shadow_geom);
    auto fs = assets->find(CoreAsset::shadow_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto gssrc = assets->request(platform, gs);
    auto fssrc = assets->request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

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

    VkSpecializationMapEntry specializationmap[1] = {};
    specializationmap[0] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.shadowpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.modelshadowpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.actorpipeline == 0)
  {
    //
    // Actor Pipeline
    //

    auto vs = assets->find(CoreAsset::actor_vert);
    auto fs = assets->find(CoreAsset::geometry_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexattributes[0] = context.vertexattributes[0];
    vertexattributes[1] = context.vertexattributes[1];
    vertexattributes[2] = context.vertexattributes[2];
    vertexattributes[3] = context.vertexattributes[3];
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

    VkPipelineColorBlendAttachmentState blendattachments[3] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

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
    pipelineinfo.renderPass = context.geometrypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.actorpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.actorshadowpipeline == 0)
  {
    //
    // Actor Shadow Pipeline
    //

    auto vs = assets->find(CoreAsset::actor_shadow_vert);
    auto gs = assets->find(CoreAsset::shadow_geom);
    auto fs = assets->find(CoreAsset::shadow_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto gssrc = assets->request(platform, gs);
    auto fssrc = assets->request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[2] = {};
    vertexbindings[0].binding = 0;
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexbindings[1].binding = 1;
    vertexbindings[1].stride = 32;
    vertexbindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexattributes[6] = {};
    vertexattributes[0] = context.vertexattributes[0];
    vertexattributes[1] = context.vertexattributes[1];
    vertexattributes[2] = context.vertexattributes[2];
    vertexattributes[3] = context.vertexattributes[3];
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

    VkSpecializationMapEntry specializationmap[1] = {};
    specializationmap[0] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.shadowpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.actorshadowpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.translucentpipeline == 0)
  {
    //
    // Translucent Pipeline
    //

    auto vs = assets->find(CoreAsset::translucent_vert);
    auto fs = assets->find(CoreAsset::translucent_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
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

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[1] = { ShaderLocation::MaxPointLights, offsetof(ComputeConstants, MaxPointLights), sizeof(ComputeConstants::MaxPointLights) };
    specializationmap[2] = { ShaderLocation::MaxTileLights, offsetof(ComputeConstants, MaxTileLights), sizeof(ComputeConstants::MaxTileLights) };
    specializationmap[3] = { ShaderLocation::MaxEnvironments, offsetof(ComputeConstants, MaxEnvironments), sizeof(ComputeConstants::MaxEnvironments) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.translucentpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.ssaopipeline == 0)
  {
    //
    // SSAO Pipeline
    //

    auto cs = assets->find(CoreAsset::ssao_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[5] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, SSAOSizeX), sizeof(ComputeConstants::SSAOSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, SSAOSizeY), sizeof(ComputeConstants::SSAOSizeY) };
    specializationmap[2] = { ShaderLocation::NoiseSize, offsetof(ComputeConstants, NoiseSize), sizeof(ComputeConstants::NoiseSize) };
    specializationmap[3] = { ShaderLocation::KernelSize, offsetof(ComputeConstants, KernelSize), sizeof(ComputeConstants::KernelSize) };
    specializationmap[4] = { ShaderLocation::SSAORadius, offsetof(ComputeConstants, SSAORadius), sizeof(ComputeConstants::SSAORadius) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.ssaopipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);

    mt19937 random(random_device{}());
    uniform_real_distribution<float> unit(0.0f, 1.0f);

    SSAOSet ssaoset;

    for(size_t i = 0; i < extentof(ssaoset.noise); ++i)
    {
      ssaoset.noise[i] = Vec4(normalise(Vec2(2*unit(random)-1, 2*unit(random)-1)), unit(random), unit(random));
    }

    for(size_t i = 0; i < extentof(ssaoset.kernel); ++i)
    {
      ssaoset.kernel[i] = normalise(Vec4(2*unit(random)-1, 2*unit(random)-1, unit(random), 0.0f)) * lerp(0.1f, 1.0f, pow(i / (float)extentof(ssaoset.kernel), 2.0f));
    }

    context.ssaoset = create_storagebuffer(context.vulkan, sizeof(ssaoset), &ssaoset);
  }

  if (context.lightingpipeline == 0)
  {
    //
    // Lighting Pipeline
    //

    auto cs = assets->find(CoreAsset::lighting_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[6] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, LightingSizeX), sizeof(ComputeConstants::LightingSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, LightingSizeY), sizeof(ComputeConstants::LightingSizeY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::MaxPointLights, offsetof(ComputeConstants, MaxPointLights), sizeof(ComputeConstants::MaxPointLights) };
    specializationmap[4] = { ShaderLocation::MaxTileLights, offsetof(ComputeConstants, MaxTileLights), sizeof(ComputeConstants::MaxTileLights) };
    specializationmap[5] = { ShaderLocation::MaxEnvironments, offsetof(ComputeConstants, MaxEnvironments), sizeof(ComputeConstants::MaxEnvironments) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.lightingpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.skyboxpipeline == 0)
  {
    //
    // SkyBox Pipeline
    //

    auto vs = assets->find(CoreAsset::skybox_vert);
    auto fs = assets->find(CoreAsset::skybox_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

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
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.skyboxpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);

    context.skyboxdescriptors[0] = allocate_descriptorset(context.vulkan, context.descriptorpool, context.scenesetlayout, context.sceneset, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
    context.skyboxdescriptors[1] = allocate_descriptorset(context.vulkan, context.descriptorpool, context.scenesetlayout, context.sceneset, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);

    context.skyboxcommands[0] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    context.skyboxcommands[1] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
  }

  if (context.fogpipeline == 0)
  {
    //
    // Fog Pipeline
    //

    auto vs = assets->find(CoreAsset::fogplane_vert);
    auto fs = assets->find(CoreAsset::fogplane_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
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

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[1] = { ShaderLocation::MaxPointLights, offsetof(ComputeConstants, MaxPointLights), sizeof(ComputeConstants::MaxPointLights) };
    specializationmap[2] = { ShaderLocation::MaxTileLights, offsetof(ComputeConstants, MaxTileLights), sizeof(ComputeConstants::MaxTileLights) };
    specializationmap[3] = { ShaderLocation::MaxEnvironments, offsetof(ComputeConstants, MaxEnvironments), sizeof(ComputeConstants::MaxEnvironments) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.fogpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.oceanpipeline == 0)
  {
    //
    // Ocean Pipeline
    //

    auto vs = assets->find(CoreAsset::ocean_vert);
    auto fs = assets->find(CoreAsset::ocean_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[3] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

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
    pipelineinfo.renderPass = context.geometrypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.oceanpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.particlepipeline[0] == 0)
  {
    //
    // Particle Pipeline (Soft Additive)
    //

    auto vs = assets->find(CoreAsset::particle_vert);
    auto fs = assets->find(CoreAsset::particle_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
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

    VkSpecializationMapEntry specializationmap[5] = {};
    specializationmap[0] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[1] = { ShaderLocation::MaxPointLights, offsetof(ComputeConstants, MaxPointLights), sizeof(ComputeConstants::MaxPointLights) };
    specializationmap[2] = { ShaderLocation::MaxTileLights, offsetof(ComputeConstants, MaxTileLights), sizeof(ComputeConstants::MaxTileLights) };
    specializationmap[3] = { ShaderLocation::MaxEnvironments, offsetof(ComputeConstants, MaxEnvironments), sizeof(ComputeConstants::MaxEnvironments) };
    specializationmap[4] = { ShaderLocation::SoftParticles, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.particlepipeline[0] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.waterpipeline == 0)
  {
    //
    // Water Pipeline
    //

    auto vs = assets->find(CoreAsset::water_vert);
    auto fs = assets->find(CoreAsset::water_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
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

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[1] = { ShaderLocation::MaxPointLights, offsetof(ComputeConstants, MaxPointLights), sizeof(ComputeConstants::MaxPointLights) };
    specializationmap[2] = { ShaderLocation::MaxTileLights, offsetof(ComputeConstants, MaxTileLights), sizeof(ComputeConstants::MaxTileLights) };
    specializationmap[3] = { ShaderLocation::MaxEnvironments, offsetof(ComputeConstants, MaxEnvironments), sizeof(ComputeConstants::MaxEnvironments) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.waterpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.ssrpipeline == 0)
  {
    //
    // SSR Gen Pipeline
    //

    auto cs = assets->find(CoreAsset::ssr_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[2] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, SSRSizeX), sizeof(ComputeConstants::SSRSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, SSRSizeY), sizeof(ComputeConstants::SSRSizeY) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.ssrpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.luminancepipeline == 0)
  {
    //
    // Luminance Pipeline
    //

    auto cs = assets->find(CoreAsset::luminance_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

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

    context.luminancepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.bloompipeline[0] == 0)
  {
    //
    // Bloom Luma Pipeline
    //

    auto cs = assets->find(CoreAsset::bloom_luma_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, BloomLumaSizeX), sizeof(ComputeConstants::BloomLumaSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, BloomLumaSizeY), sizeof(ComputeConstants::BloomLumaSizeY) };
    specializationmap[2] = { ShaderLocation::BloomCutoff, offsetof(ComputeConstants, BloomCutoff), sizeof(ComputeConstants::BloomCutoff) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.bloompipeline[0] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.bloompipeline[1] == 0)
  {
    //
    // Bloom H-Blur Pipeline
    //

    auto cs = assets->find(CoreAsset::bloom_hblur_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, BloomHBlurSize), sizeof(ComputeConstants::BloomHBlurSize) };
    specializationmap[1] = { ShaderLocation::BloomBlurSigma, offsetof(ComputeConstants, BloomBlurSigma), sizeof(ComputeConstants::BloomBlurSigma) };
    specializationmap[2] = { ShaderLocation::BloomBlurRadius, offsetof(ComputeConstants, BloomBlurRadius), sizeof(ComputeConstants::BloomBlurRadius) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.bloompipeline[1] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.bloompipeline[2] == 0)
  {
    //
    // Bloom V-Blur Pipeline
    //

    auto cs = assets->find(CoreAsset::bloom_vblur_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::SizeY, offsetof(ComputeConstants, BloomVBlurSize), sizeof(ComputeConstants::BloomVBlurSize) };
    specializationmap[1] = { ShaderLocation::BloomBlurSigma, offsetof(ComputeConstants, BloomBlurSigma), sizeof(ComputeConstants::BloomBlurSigma) };
    specializationmap[2] = { ShaderLocation::BloomBlurRadius, offsetof(ComputeConstants, BloomBlurRadius), sizeof(ComputeConstants::BloomBlurRadius) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.bloompipeline[2] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.compositepipeline == 0)
  {
    //
    // Composite Pipeline
    //

    auto vs = assets->find(CoreAsset::composite_vert);
    auto fs = assets->find(CoreAsset::composite_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_FALSE;

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
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.compositepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);

    context.compositedescriptor = allocate_descriptorset(context.vulkan, context.descriptorpool, context.scenesetlayout, context.sceneset, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);

    context.compositecommands = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
  }

  if (context.spritepipeline == 0)
  {
    //
    // Sprite Pipeline
    //

    auto vs = assets->find(CoreAsset::sprite_vert);
    auto fs = assets->find(CoreAsset::sprite_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_FALSE;

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
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.spritepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.gizmopipeline == 0)
  {
    //
    // Gizmo Pipeline
    //

    auto vs = assets->find(CoreAsset::gizmo_vert);
    auto fs = assets->find(CoreAsset::gizmo_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

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
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.gizmopipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.wireframepipeline == 0)
  {
    //
    // WireFrame Pipeline
    //

    auto vs = assets->find(CoreAsset::wireframe_vert);
    auto gs = assets->find(CoreAsset::wireframe_geom);
    auto fs = assets->find(CoreAsset::wireframe_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto gssrc = assets->request(platform, gs);
    auto fssrc = assets->request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
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

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.renderpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.wireframepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.stencilmaskpipeline == 0)
  {
    //
    // Stencil Mask Pipeline
    //

    auto vs = assets->find(CoreAsset::stencilmask_vert);
    auto fs = assets->find(CoreAsset::stencilmask_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_FALSE;
    blendattachments[0].colorWriteMask = 0;//VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_FALSE;
    depthstate.stencilTestEnable = VK_TRUE;
    depthstate.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depthstate.front.passOp = VK_STENCIL_OP_REPLACE;
    depthstate.front.writeMask = 0xFF;
    depthstate.front.reference = 1;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };

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
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.stencilmaskpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.stencilfillpipeline == 0)
  {
    //
    // Stencil Fill Pipeline
    //

    auto vs = assets->find(CoreAsset::stencilfill_vert);
    auto fs = assets->find(CoreAsset::stencilfill_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

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
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;
    depthstate.stencilTestEnable = VK_TRUE;
    depthstate.front.compareOp = VK_COMPARE_OP_EQUAL;
    depthstate.front.compareMask = 0xFF;
    depthstate.front.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
    depthstate.front.writeMask = 0xFF;
    depthstate.front.reference = 1;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };

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
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.stencilfillpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.stencilpathpipeline == 0)
  {
    //
    // Stencil Path Pipeline
    //

    auto vs = assets->find(CoreAsset::stencilpath_vert);
    auto gs = assets->find(CoreAsset::stencilpath_geom);
    auto fs = assets->find(CoreAsset::stencilpath_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto gssrc = assets->request(platform, gs);
    auto fssrc = assets->request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;
    depthstate.stencilTestEnable = VK_TRUE;
    depthstate.front.compareOp = VK_COMPARE_OP_GREATER;
    depthstate.front.compareMask = 0xFF;
    depthstate.front.passOp = VK_STENCIL_OP_REPLACE;
    depthstate.front.writeMask = 0xFF;
    depthstate.front.reference = 1;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.renderpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.stencilpathpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.linepipeline == 0)
  {
    //
    // Line Pipeline
    //

    auto vs = assets->find(CoreAsset::line_vert);
    auto gs = assets->find(CoreAsset::line_geom);
    auto fs = assets->find(CoreAsset::line_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto gssrc = assets->request(platform, gs);
    auto fssrc = assets->request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
//    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
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

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.renderpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.linepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.outlinepipeline == 0)
  {
    //
    // Outline Pipeline
    //

    auto vs = assets->find(CoreAsset::outline_vert);
    auto gs = assets->find(CoreAsset::outline_geom);
    auto fs = assets->find(CoreAsset::outline_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto gssrc = assets->request(platform, gs);
    auto fssrc = assets->request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
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

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.renderpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.outlinepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.envbrdf == 0)
  {
    auto image = assets->find(CoreAsset::envbrdf_lut);

    if (!image)
      return false;

    asset_guard lock(assets);

    auto bits = assets->request(platform, image);

    if (!bits)
      return false;

    context.envbrdf = create_texture(context.vulkan, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, bits, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
  }

  if (context.whitediffuse == 0)
  {
    auto image = assets->find(CoreAsset::white_diffuse);

    if (!image)
      return false;

    asset_guard lock(assets);

    auto bits = assets->request(platform, image);

    if (!bits)
      return false;

    context.whitediffuse = create_texture(context.vulkan, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, bits);
  }

  if (context.nominalnormal == 0)
  {
    auto image = assets->find(CoreAsset::nominal_normal);

    if (!image)
      return false;

    asset_guard lock(assets);

    auto bits = assets->request(platform, image);

    if (!bits)
      return false;

    context.nominalnormal = create_texture(context.vulkan, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, bits);
  }

  if (context.unitquad == 0)
  {
    auto mesh = assets->find(CoreAsset::unit_quad);

    if (!mesh)
      return false;

    asset_guard lock(assets);

    auto bits = assets->request(platform, mesh);

    if (!bits)
      return false;

    context.unitquad = create_vertexbuffer(context.vulkan, context.transferbuffer, bits, mesh->vertexcount, sizeof(PackVertex));
  }

  context.width = 0;
  context.height = 0;

  context.ready = true;

  return true;
}


///////////////////////// prepare_render_pipeline /////////////////////////
bool prepare_render_pipeline(RenderContext &context, RenderParams const &params)
{
  bool dirty = false;
  dirty |= (context.width != params.width || context.height != params.height || context.scale != params.scale || context.aspect != params.aspect);
  dirty |= (context.ssaoscale != params.ssaoscale);

  if (dirty)
  {
    assert(context.ready);

    release_render_pipeline(context);

    context.width = params.width;
    context.height = params.height;
    context.scale = params.scale;
    context.aspect = params.aspect;

    context.fbox = (context.width - min((int)(context.height * params.aspect), context.width))/2;
    context.fboy = (context.height - min((int)(context.width / params.aspect), context.height))/2;
    context.fbowidth = (int)((context.width - 2 * context.fbox) * context.scale);
    context.fboheight = (int)((context.height - 2 * context.fboy) * context.scale);

    if (context.fbowidth == 0 || context.fboheight == 0)
      return false;

    CommandPool setuppool = create_commandpool(context.vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    CommandBuffer setupbuffer = allocate_commandbuffer(context.vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    begin(context.vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    //
    // Shadow Map
    //

    context.shadows.shadowmap = create_texture(context.vulkan, setupbuffer, context.shadows.width, context.shadows.height, context.shadows.nslices, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    VkSamplerCreateInfo shadowsamplerinfo = {};
    shadowsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    shadowsamplerinfo.magFilter = VK_FILTER_LINEAR;
    shadowsamplerinfo.minFilter = VK_FILTER_LINEAR;
    shadowsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowsamplerinfo.compareEnable = VK_TRUE;
    shadowsamplerinfo.compareOp = VK_COMPARE_OP_LESS;

    context.shadows.shadowmap.sampler = create_sampler(context.vulkan, shadowsamplerinfo);

    //
    // Shadow Buffer
    //

    VkImageView shadowbuffer[1] = {};
    shadowbuffer[0] = context.shadows.shadowmap.imageview;

    VkFramebufferCreateInfo shadowbufferinfo = {};
    shadowbufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    shadowbufferinfo.renderPass = context.shadowpass;
    shadowbufferinfo.attachmentCount = extentof(shadowbuffer);
    shadowbufferinfo.pAttachments = shadowbuffer;
    shadowbufferinfo.width = context.shadows.width;
    shadowbufferinfo.height = context.shadows.height;
    shadowbufferinfo.layers = context.shadows.nslices;

    context.shadowbuffer = create_framebuffer(context.vulkan, shadowbufferinfo);

    //
    // Color Attachment
    //

    context.colorbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_GENERAL);

    clear(setupbuffer, context.colorbuffer.image, VK_IMAGE_LAYOUT_GENERAL, Color4(0.0, 0.0, 0.0, 0.0));

    //
    // Geometry Attachment
    //

    context.rt0buffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    context.rt1buffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    context.normalbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    //
    // Depth Attachment
    //

    context.depthbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    VkSamplerCreateInfo depthsamplerinfo = {};
    depthsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    depthsamplerinfo.magFilter = VK_FILTER_LINEAR;
    depthsamplerinfo.minFilter = VK_FILTER_LINEAR;
    depthsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    depthsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    depthsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    depthsamplerinfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    context.depthbuffer.sampler = create_sampler(context.vulkan, depthsamplerinfo);

    //
    // Geometry Buffer
    //

    VkImageView geometrybuffer[4] = {};
    geometrybuffer[0] = context.rt0buffer.imageview;
    geometrybuffer[1] = context.rt1buffer.imageview;
    geometrybuffer[2] = context.normalbuffer.imageview;
    geometrybuffer[3] = context.depthbuffer.imageview;

    VkFramebufferCreateInfo geometrybufferinfo = {};
    geometrybufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    geometrybufferinfo.renderPass = context.geometrypass;
    geometrybufferinfo.attachmentCount = extentof(geometrybuffer);
    geometrybufferinfo.pAttachments = geometrybuffer;
    geometrybufferinfo.width = context.fbowidth;
    geometrybufferinfo.height = context.fboheight;
    geometrybufferinfo.layers = 1;

    context.geometrybuffer = create_framebuffer(context.vulkan, geometrybufferinfo);

    //
    // Forward Buffer
    //

    VkImageView forwardbuffer[2] = {};
    forwardbuffer[0] = context.colorbuffer.imageview;
    forwardbuffer[1] = context.depthbuffer.imageview;

    VkFramebufferCreateInfo forwardbufferinfo = {};
    forwardbufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    forwardbufferinfo.renderPass = context.forwardpass;
    forwardbufferinfo.attachmentCount = extentof(forwardbuffer);
    forwardbufferinfo.pAttachments = forwardbuffer;
    forwardbufferinfo.width = context.fbowidth;
    forwardbufferinfo.height = context.fboheight;
    forwardbufferinfo.layers = 1;

    context.forwardbuffer = create_framebuffer(context.vulkan, forwardbufferinfo);

    //
    // Render Target
    //

    context.rendertarget = create_texture(context.vulkan, setupbuffer, context.width, context.height, 1, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    context.depthstencil = create_texture(context.vulkan, setupbuffer, context.width, context.height, 1, 1, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    //
    // Frame Buffer
    //

    VkImageView framebuffer[2] = {};
    framebuffer[0] = context.rendertarget.imageview;
    framebuffer[1] = context.depthstencil.imageview;

    VkFramebufferCreateInfo framebufferinfo = {};
    framebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferinfo.renderPass = context.renderpass;
    framebufferinfo.attachmentCount = extentof(framebuffer);
    framebufferinfo.pAttachments = framebuffer;
    framebufferinfo.width = context.width;
    framebufferinfo.height = context.height;
    framebufferinfo.layers = 1;

    context.framebuffer = create_framebuffer(context.vulkan, framebufferinfo);

    //
    // Scratch Buffers
    //

    context.scratchbuffers[0] = create_texture(context.vulkan, setupbuffer, context.fbowidth/2, context.fboheight/2, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_GENERAL);
    context.scratchbuffers[1] = create_texture(context.vulkan, setupbuffer, context.fbowidth/2, context.fboheight/2, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_GENERAL);
    context.scratchbuffers[2] = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_GENERAL);

    for(size_t i = 0; i < extentof(context.scratchbuffers); ++i)
    {
      clear(setupbuffer, context.scratchbuffers[i].image, VK_IMAGE_LAYOUT_GENERAL, Color4(0.0, 0.0, 0.0, 0.0));
    }

    //
    // SSAO
    //

    context.ssaobuffers[0] = create_texture(context.vulkan, setupbuffer, max(int(context.fbowidth*params.ssaoscale), 1), max(int(context.fboheight*params.ssaoscale), 1), 1, 1, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_GENERAL);
    context.ssaobuffers[1] = create_texture(context.vulkan, setupbuffer, max(int(context.fbowidth*params.ssaoscale), 1), max(int(context.fboheight*params.ssaoscale), 1), 1, 1, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_GENERAL);

    for(size_t i = 0; i < extentof(context.ssaobuffers); ++i)
    {
      clear(setupbuffer, context.ssaobuffers[i].image, VK_IMAGE_LAYOUT_GENERAL, Color4(1.0, 1.0, 1.0, 1.0));
    }

    context.ssaoscale = params.ssaoscale;

    //
    // Scene Descriptor
    //

    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::rt0, context.rt0buffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::rt1, context.rt1buffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::normalmap, context.normalbuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::depthmap, context.depthbuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::shadowmap, context.shadows.shadowmap);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::envbrdf, context.envbrdf);

    //
    // Frame Descriptor
    //

    for(size_t i = 0; i < extentof(context.framedescriptors); ++i)
    {
      bind_buffer(context.vulkan, context.framedescriptors[i], ShaderLocation::lumaset, context.transferbuffer, 0, sizeof(LumaSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::colormap, context.colorbuffer);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchmap0, context.scratchbuffers[0]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchmap1, context.scratchbuffers[1]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchmap2, context.scratchbuffers[2]);

      bind_buffer(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaoset, context.ssaoset, 0, sizeof(SSAOSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaomap, context.ssaobuffers[i]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaoprevmap, context.ssaobuffers[i ^ 1]);
      bind_imageview(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaotarget, context.ssaobuffers[i].imageview);

      bind_imageview(context.vulkan, context.framedescriptors[i], ShaderLocation::colortarget, context.colorbuffer.imageview);
      bind_imageview(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchtarget0, context.scratchbuffers[0].imageview);
      bind_imageview(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchtarget1, context.scratchbuffers[1].imageview);
      bind_imageview(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchtarget2, context.scratchbuffers[2].imageview);
    }

    //
    // Composite
    //

    bind_texture(context.vulkan, context.compositedescriptor, ShaderLocation::rt0, context.rt0buffer);
    bind_texture(context.vulkan, context.compositedescriptor, ShaderLocation::rt1, context.rt1buffer);
    bind_texture(context.vulkan, context.compositedescriptor, ShaderLocation::normalmap, context.normalbuffer);
    bind_texture(context.vulkan, context.compositedescriptor, ShaderLocation::depthmap, context.depthbuffer);
    bind_texture(context.vulkan, context.compositedescriptor, ShaderLocation::colorsrc, context.colorbuffer);
    bind_texture(context.vulkan, context.compositedescriptor, ShaderLocation::ssrsrc, context.scratchbuffers[2]);
    bind_texture(context.vulkan, context.compositedescriptor, ShaderLocation::bloomsrc, context.scratchbuffers[0]);

    begin(context.vulkan, context.compositecommands, context.framebuffer, context.renderpass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
    bind_pipeline(context.compositecommands, context.compositepipeline, context.fbox, context.fboy, context.width-2*context.fbox, context.height-2*context.fboy, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_descriptor(context.compositecommands, context.compositedescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_vertexbuffer(context.compositecommands, 0, context.unitquad);
    draw(context.compositecommands, context.unitquad.vertexcount, 1, 0, 0);
    end(context.vulkan, context.compositecommands);

    // Finalise

    end(context.vulkan, setupbuffer);

    submit(context.vulkan, setupbuffer);

    vkQueueWaitIdle(context.vulkan.queue);

    context.prepared = true;
  }

  return context.prepared;
}


///////////////////////// release_render_pipeline ///////////////////////////
void release_render_pipeline(RenderContext &context)
{
  vkDeviceWaitIdle(context.vulkan);
  vkResetCommandBuffer(context.commandbuffers[0], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  vkResetCommandBuffer(context.commandbuffers[1], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

  context.prepared = false;

  context.shadowbuffer = {};

  context.rendertarget = {};
  context.depthstencil = {};

  context.rt0buffer = {};
  context.rt1buffer = {};
  context.normalbuffer = {};
  context.depthbuffer = {};
  context.colorbuffer = {};
  context.rendertarget = {};
  context.depthstencil = {};

  context.geometrybuffer = {};
  context.forwardbuffer = {};
  context.framebuffer = {};

  context.scratchbuffers[0] = {};
  context.scratchbuffers[1] = {};
  context.scratchbuffers[2] = {};

  context.ssaobuffers[0] = {};
  context.ssaobuffers[1] = {};

  context.shadows.shadowmap = {};

  context.width = 0;
  context.height = 0;
}


///////////////////////// prepare_shadowview ////////////////////////////////
void prepare_shadowview(ShadowMap &shadowmap, Camera const &camera, Vec3 const &lightdirection)
{
  const float lambda = shadowmap.shadowsplitlambda;
  const float znear = 0.1f;
  const float zfar = shadowmap.shadowsplitfar;
  const float extrusion = 1000.0f;
  constexpr int nsplits = ShadowMap::nslices;

  float splits[nsplits+1] = { znear };

  for(int i = 1; i <= nsplits; ++i)
  {
    float alpha = (float)i / nsplits;
    float logdist = znear * pow(zfar / znear, alpha);
    float uniformdist = znear + (zfar -znear) * alpha;

    splits[i] = lerp(uniformdist, logdist, lambda);
  }

  auto up = Vec3(0, 1, 0);

  auto snapview = Transform::lookat(Vec3(0,0,0), -lightdirection, up);

  for(int i = 0; i < nsplits; ++i)
  {
    auto frustum = camera.frustum(splits[i], splits[i+1] + 1.0f);

    auto frustumradius = 0.5f * norm(frustum.corners[0] - frustum.corners[6]);

    auto frustumcentre = frustum.centre();

    frustumcentre = inverse(snapview) * frustumcentre;
    frustumcentre.x -= fmod(frustumcentre.x, (frustumradius + frustumradius) / shadowmap.width);
    frustumcentre.y -= fmod(frustumcentre.y, (frustumradius + frustumradius) / shadowmap.height);
    frustumcentre = snapview * frustumcentre;

    auto lightpos = frustumcentre - extrusion * lightdirection;

    auto lightview = Transform::lookat(lightpos, lightpos + lightdirection, up);

    auto lightproj = OrthographicProjection(-frustumradius, -frustumradius, frustumradius, frustumradius, 0.1f, extrusion + frustumradius) * ScaleMatrix(1.0f, -1.0f, 1.0f, 1.0f);

    shadowmap.splits[i] = splits[i+1];
    shadowmap.shadowview[i] = lightproj * inverse(lightview).matrix();
  }
}


///////////////////////// prepare_sceneset //////////////////////////////////
void prepare_sceneset(RenderContext &context, PushBuffer const &renderables, RenderParams const &params)
{
  SceneSet sceneset;

  sceneset.proj = context.proj;
  sceneset.invproj = inverse(sceneset.proj);
  sceneset.view = context.view;
  sceneset.invview = inverse(sceneset.view);
  sceneset.worldview = context.proj * context.view;
  sceneset.prevview = context.prevcamera.view();
  sceneset.skyview = (inverse(params.skyboxorientation) * Transform::rotation(context.camera.rotation())).matrix() * sceneset.invproj;

  sceneset.viewport = Vec4(context.fbox, context.fboy, context.width - 2*context.fbox, context.height - 2*context.fboy);

  sceneset.camera.position = context.camera.position();
  sceneset.camera.exposure = context.camera.exposure();
  sceneset.camera.skyboxlod = params.skyboxlod;
  sceneset.camera.ssrstrength = params.ssrstrength;
  sceneset.camera.bloomstrength = params.bloomstrength;

  assert(sizeof(sceneset.shadowview) <= sizeof(context.shadows.shadowview));
  memcpy(sceneset.splits, context.shadows.splits.data(), sizeof(context.shadows.splits));
  memcpy(sceneset.shadowview, context.shadows.shadowview.data(), sizeof(context.shadows.shadowview));

  auto &mainlight = sceneset.mainlight;
  auto &pointlights = sceneset.pointlights;

  mainlight.direction = params.sundirection;
  mainlight.intensity = params.sunintensity;

  auto &pointlightcount = sceneset.pointlightcount = 0;

  for(auto &renderable : renderables)
  {
    if (renderable.type == Renderable::Type::Lights)
    {
      auto lights = renderable_cast<Renderable::Lights>(&renderable)->lightlist;

      for(size_t i = 0; lights && i < lights->pointlightcount && pointlightcount < extentof(pointlights); ++i)
      {
        pointlights[pointlightcount].position = lights->pointlights[i].position;
        pointlights[pointlightcount].intensity = lights->pointlights[i].intensity;
        pointlights[pointlightcount].attenuation = lights->pointlights[i].attenuation;
        pointlights[pointlightcount].attenuation.w = params.lightfalloff * lights->pointlights[i].attenuation.w;

        ++pointlightcount;
      }
    }
  }

  auto &envcount = sceneset.envcount = 0;

  VkDescriptorImageInfo imageinfos[6] = {};

  for(auto &renderable : renderables)
  {
    if (renderable.type == Renderable::Type::Environment)
    {
      auto &environment = *renderable_cast<Renderable::Environment>(&renderable);

      if (envcount + 1 < extentof(imageinfos))
      {
        assert(environment.envmap && environment.envmap->ready());

        imageinfos[envcount].sampler = environment.envmap->texture.sampler;
        imageinfos[envcount].imageView = environment.envmap->texture.imageview;
        imageinfos[envcount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        sceneset.environments[envcount].halfdim = environment.dimension/2;
        sceneset.environments[envcount].invtransform = inverse(environment.transform);

        envcount += 1;
      }
    }
  }

  if (params.skybox)
  {
    assert(params.skybox->ready());

    imageinfos[envcount].sampler = params.skybox->texture.sampler;
    imageinfos[envcount].imageView = params.skybox->texture.imageview;
    imageinfos[envcount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    sceneset.environments[envcount].halfdim = Vec3(1e5f, 1e5f, 1e5f);
    sceneset.environments[envcount].invtransform = inverse(params.skyboxorientation);

    envcount += 1;
  }

  if (envcount != 0)
  {
    bind_texture(context.vulkan, context.framedescriptors[context.frame & 1], ShaderLocation::envmaps, imageinfos, envcount);
  }

  update(context.commandbuffers[context.frame & 1], context.sceneset, 0, sizeof(sceneset), &sceneset);
}


///////////////////////// draw_calls ////////////////////////////////////////
extern void draw_geometry(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Geometry const &geometry);
extern void draw_objects(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Objects const &objects);
extern void draw_casters(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Casters const &casters);
extern void draw_skybox(RenderContext &context, VkCommandBuffer commandbuffer, RenderParams const &params);
extern void draw_sprites(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Sprites const &sprites);
extern void draw_overlays(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Overlays const &overlays);

///////////////////////// render_fallback ///////////////////////////////////
void render_fallback(RenderContext &context, DatumPlatform::Viewport const &viewport, void *bitmap, int width, int height)
{
  assert(context.vulkan);

  CommandBuffer &commandbuffer = context.commandbuffers[context.frame & 1];

  wait_fence(context.vulkan, context.framefence);

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  transition_acquire(commandbuffer, viewport.image);

  clear(commandbuffer, viewport.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Color4(0.0f, 0.0f, 0.0f, 1.0f));

  if (bitmap)
  {
    size_t size = width * height * sizeof(uint32_t);

    assert(size < TransferBufferSize);

    memcpy(map_memory<uint8_t>(context.vulkan, context.transferbuffer, 0, size), bitmap, size);

    blit(commandbuffer, context.transferbuffer, 0, width, height, viewport.image, max(viewport.width - width, 0)/2, max(viewport.height - height, 0)/2, width, height);
  }

  transition_present(commandbuffer, viewport.image);

  end(context.vulkan, commandbuffer);

  submit(context.vulkan, commandbuffer, viewport.acquirecomplete, viewport.rendercomplete, context.framefence);

  vkQueueWaitIdle(context.vulkan.queue);

  context.luminance = 1.0f;

  ++context.frame;
}


///////////////////////// render ////////////////////////////////////////////
void render(RenderContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params)
{
  assert(context.ready);
  assert(context.prepared);

  context.camera = camera;
  context.proj = camera.proj();
  context.view = camera.view();

  auto &commandbuffer = context.commandbuffers[context.frame & 1];

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  reset_querypool(commandbuffer, context.timingquerypool, 0, 1);

  prepare_shadowview(context.shadows, camera, params.sundirection);

  prepare_sceneset(context, renderables, params);

  VkClearValue clearvalues[4];
  clearvalues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
  clearvalues[1].color = { 0.0f, 0.0f, 0.0f, 0.0f };
  clearvalues[2].color = { 0.0f, 0.0f, 0.0f, 0.0f };
  clearvalues[3].depthStencil = { 1, 0 };

  bind_descriptor(commandbuffer, context.scenedescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);
  bind_descriptor(commandbuffer, context.framedescriptors[context.frame & 1], context.pipelinelayout, ShaderLocation::computeset, VK_PIPELINE_BIND_POINT_COMPUTE);

  querytimestamp(commandbuffer, context.timingquerypool, 0);

  //
  // Shadows
  //

  beginpass(commandbuffer, context.shadowpass, context.shadowbuffer, 0, 0, context.shadows.width, context.shadows.height, 1, &clearvalues[3]);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Casters:
        draw_casters(context, commandbuffer, *renderable_cast<Renderable::Casters>(&renderable));
        break;

      default:
        break;
    }
  }

  endpass(commandbuffer, context.shadowpass);

  querytimestamp(commandbuffer, context.timingquerypool, 1);

  //
  // Geometry
  //

  beginpass(commandbuffer, context.geometrypass, context.geometrybuffer, 0, 0, context.fbowidth, context.fboheight, 4, &clearvalues[0]);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Geometry:
        draw_geometry(context, commandbuffer, *renderable_cast<Renderable::Geometry>(&renderable));
        break;

      default:
        break;
    }
  }

  endpass(commandbuffer, context.geometrypass);

  querytimestamp(commandbuffer, context.timingquerypool, 2);

  //
  // Lighting
  //

  if (params.ssaoscale != 0)
  {
    bind_pipeline(commandbuffer, context.ssaopipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.ssaobuffers[0].width, context.ssaobuffers[0].height, 1, computeconstants.SSAODispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 3);

  bind_pipeline(commandbuffer, context.lightingpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, context.fbowidth, context.fboheight, 1, computeconstants.LightingDispatch);

  querytimestamp(commandbuffer, context.timingquerypool, 4);

  //
  // Forward
  //

  beginpass(commandbuffer, context.forwardpass, context.forwardbuffer, 0, 0, context.fbowidth, context.fboheight, 0, nullptr);

  if (params.skybox)
  {
    draw_skybox(context, commandbuffer, params);
  }

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Objects:
        draw_objects(context, commandbuffer, *renderable_cast<Renderable::Objects>(&renderable));
        break;

      default:
        break;
    }
  }

  endpass(commandbuffer, context.forwardpass);

  querytimestamp(commandbuffer, context.timingquerypool, 5);

  //
  // SSR
  //

  if (params.ssrstrength != 0)
  {
    bind_pipeline(commandbuffer, context.ssrpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth, context.fboheight, 1, computeconstants.SSRDispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 6);

  //
  // Luminance
  //

  bind_pipeline(commandbuffer, context.luminancepipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, 1, 1, 1);

  querytimestamp(commandbuffer, context.timingquerypool, 7);

  //
  // Bloom
  //

  if (params.bloomstrength != 0)
  {
    bind_pipeline(commandbuffer, context.bloompipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth/2, context.fboheight/2, 1, computeconstants.BloomLumaDispatch);

    bind_pipeline(commandbuffer, context.bloompipeline[1], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth/2, context.fboheight/2, 1, computeconstants.BloomHBlurDispatch);

    bind_pipeline(commandbuffer, context.bloompipeline[2], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth/2, context.fboheight/2, 1, computeconstants.BloomVBlurDispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 8);

  //
  // Overlay
  //

  beginpass(commandbuffer, context.renderpass, context.framebuffer, 0, 0, context.width, context.height, 2, &clearvalues[2]);

  execute(commandbuffer, context.compositecommands);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Sprites:
        draw_sprites(context, commandbuffer, *renderable_cast<Renderable::Sprites>(&renderable));
        break;

      case Renderable::Type::Overlays:
        draw_overlays(context, commandbuffer, *renderable_cast<Renderable::Overlays>(&renderable));
        break;

      default:
        break;
    }
  }

  endpass(commandbuffer, context.renderpass);

  querytimestamp(commandbuffer, context.timingquerypool, 9);

  //
  // Blit
  //

  if (viewport.image)
  {
    transition_acquire(commandbuffer, viewport.image);

    blit(commandbuffer, context.rendertarget.image, 0, 0, context.rendertarget.width, context.rendertarget.height, viewport.image, viewport.x, viewport.y, viewport.width, viewport.height, VK_FILTER_LINEAR);

    transition_present(commandbuffer, viewport.image);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 10);

  barrier(commandbuffer);

  end(context.vulkan, commandbuffer);

  //
  // Submit
  //

  BEGIN_TIMED_BLOCK(Wait, Color3(0.1f, 0.1f, 0.1f))

  wait_fence(context.vulkan, context.framefence);

  END_TIMED_BLOCK(Wait)

  // Feedback

  context.luminance = map_memory<LumaSet>(context.vulkan, context.transferbuffer, 0, sizeof(LumaSet))->luminance;

  // Timing Queries

  uint64_t timings[16];
  retreive_querypool(context.vulkan, context.timingquerypool, 0, 11, timings);

  GPU_TIMED_BLOCK(Shadows, Color3(0.0f, 0.4f, 0.0f), timings[0], timings[1])
  GPU_TIMED_BLOCK(Geometry, Color3(0.4f, 0.0f, 0.4f), timings[1], timings[2])
  GPU_TIMED_BLOCK(SSAO, Color3(0.2f, 0.8f, 0.2f), timings[2], timings[3])
  GPU_TIMED_BLOCK(Lighting, Color3(0.0f, 0.6f, 0.4f), timings[3], timings[4])
  GPU_TIMED_BLOCK(Forward, Color3(0.0f, 0.4f, 0.8f), timings[4], timings[5])
  GPU_TIMED_BLOCK(SSR, Color3(0.2f, 0.4f, 0.8f), timings[5], timings[6])
  GPU_TIMED_BLOCK(Luminance, Color3(0.8f, 0.4f, 0.2f), timings[6], timings[7])
  GPU_TIMED_BLOCK(Bloom, Color3(0.2f, 0.2f, 0.6f), timings[7], timings[8])
  GPU_TIMED_BLOCK(Overlay, Color3(0.4f, 0.4f, 0.0f), timings[8], timings[9])
  GPU_TIMED_BLOCK(Blit, Color3(0.4f, 0.4f, 0.4f), timings[9], timings[10])

  GPU_SUBMIT();

  submit(context.vulkan, commandbuffer, viewport.acquirecomplete, viewport.rendercomplete, context.framefence);

  context.prevcamera = camera;

  ++context.frame;
}

