//
// Datum - renderer
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "renderer.h"
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
  frameset = 3,

  scenebuf = 0,
  colormap = 1,
  diffusemap = 2,
  specularmap = 3,
  normalmap = 4,
  depthmap = 5,
  depthmipmap = 6,

  colortarget = 0,
  depthmiptarget = 1,
  depthattachment = 2,
  ssaomap = 3,
  shadowmap = 4,
  envbrdf = 5,
  envmaps = 6,
  spotmaps = 7,
  ssaobuf = 8,
  ssaoprevmap = 9,
  ssaotarget = 10,
  scratchmap0 = 11,
  scratchmap1 = 12,
  scratchmap2 = 13,
  scratchtarget0 = 14,
  scratchtarget1 = 15,
  scratchtarget2 = 16,
  lumabuf = 17,

  // Constant Ids

  SizeX = 1,
  SizeY = 2,
  SizeZ = 3,

  ClusterTileX = 4,
  ClusterTileY = 5,

  ShadowSlices = 6,

  NoiseSize = 62,
  KernelSize = 63,

  SSAORadius = 67,

  BloomCutoff = 81,
  BloomBlurRadius = 84,
  BloomBlurSigma = 85,

  DepthMipLayer = 23,

  SoftParticles = 28,
};

struct MainLight
{
  alignas(16) Vec3 direction;
  alignas(16) Color3 intensity;

  alignas( 4) float splits[4];
  alignas(16) Matrix4f shadowview[4];
};

struct PointLight
{
  alignas(16) Vec3 position;
  alignas(16) Color3 intensity;
  alignas(16) Vec4 attenuation;
};

struct SpotLight
{
  alignas(16) Vec3 position;
  alignas(16) Color3 intensity;
  alignas(16) Vec4 attenuation;
  alignas(16) Vec3 direction;
  alignas( 4) float cutoff;
  alignas(16) Transform shadowview;
};

struct Environment
{
  alignas(16) Vec3 halfdim;
  alignas(16) Transform invtransform;
};

struct CameraView
{
  alignas(16) Vec3 position;
  alignas( 4) float exposure;
  alignas( 4) float skyboxlod;
  alignas( 4) float ssrstrength;
  alignas( 4) float bloomstrength;
  alignas( 4) uint32_t frame;
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

  alignas( 4) uint32_t environmentcount;
  alignas(16) Environment environments[8];

  alignas( 4) uint32_t pointlightcount;
  alignas(16) PointLight pointlights[512];

  alignas( 4) uint32_t spotlightcount;
  alignas(16) SpotLight spotlights[16];

  // Cluster cluster[];
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

static constexpr size_t SceneBufferSize = 4*1024*1024;
static constexpr size_t TransferBufferSize = 512*1024;

static struct ComputeConstants
{
  uint32_t ShadowSlices = ShadowMap::nslices;

  uint32_t ClusterTileX = 64;
  uint32_t ClusterTileY = 64;

  uint32_t ClusterDispatch[3] = { ClusterTileX, ClusterTileY, 1 };

  uint32_t DepthMipDispatch[3] = { 16, 16, 1 };

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

  uint32_t Layers[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

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


///////////////////////// prefetch_core_assets //////////////////////////////
void prefetch_core_assets(DatumPlatform::PlatformInterface &platform, AssetManager &assets)
{
  for(int i = 1; i < CoreAsset::core_asset_count; ++i)
  {
    assets.request(platform, assets.find(i));
  }
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
bool prepare_render_context(DatumPlatform::PlatformInterface &platform, RenderContext &context, AssetManager &assets)
{
  if (context.ready)
    return true;

  assert(context.vulkan);

  if (context.descriptorpool == 0)
  {
    // DescriptorPool

    VkDescriptorPoolSize typecounts[4] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    typecounts[0].descriptorCount = 24;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[1].descriptorCount = 128;
    typecounts[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    typecounts[2].descriptorCount = 24;
    typecounts[3].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    typecounts[3].descriptorCount = 4;

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

    VkDescriptorSetLayoutBinding bindings[7] = {};
    bindings[0].binding = ShaderLocation::scenebuf;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = ShaderLocation::colormap;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[2].binding = ShaderLocation::diffusemap;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[3].binding = ShaderLocation::specularmap;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[4].binding = ShaderLocation::normalmap;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[5].binding = ShaderLocation::depthmap;
    bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].descriptorCount = 1;
    bindings[6].binding = ShaderLocation::depthmipmap;
    bindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[6].descriptorCount = 1;

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

  if (context.framesetlayout == 0)
  {
    // Frame Set

    VkDescriptorSetLayoutBinding bindings[18] = {};
    bindings[0].binding = ShaderLocation::colortarget;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = ShaderLocation::depthmiptarget;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 6;
    bindings[2].binding = ShaderLocation::depthattachment;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    bindings[2].descriptorCount = 1;
    bindings[3].binding = ShaderLocation::ssaomap;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[4].binding = ShaderLocation::shadowmap;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[5].binding = ShaderLocation::envbrdf;
    bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].descriptorCount = 1;
    bindings[6].binding = ShaderLocation::envmaps;
    bindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[6].descriptorCount = extent<decltype(SceneSet::environments)>::value;
    bindings[7].binding = ShaderLocation::spotmaps;
    bindings[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[7].descriptorCount = extent<decltype(SceneSet::spotlights)>::value;
    bindings[8].binding = ShaderLocation::ssaobuf;
    bindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[8].descriptorCount = 1;
    bindings[9].binding = ShaderLocation::ssaoprevmap;
    bindings[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[9].descriptorCount = 1;
    bindings[10].binding = ShaderLocation::ssaotarget;
    bindings[10].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[10].descriptorCount = 1;
    bindings[11].binding = ShaderLocation::scratchmap0;
    bindings[11].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[11].descriptorCount = 1;
    bindings[12].binding = ShaderLocation::scratchmap1;
    bindings[12].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[12].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[12].descriptorCount = 1;
    bindings[13].binding = ShaderLocation::scratchmap2;
    bindings[13].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[13].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[13].descriptorCount = 1;
    bindings[14].binding = ShaderLocation::scratchtarget0;
    bindings[14].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[14].descriptorCount = 1;
    bindings[15].binding = ShaderLocation::scratchtarget1;
    bindings[15].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[15].descriptorCount = 1;
    bindings[16].binding = ShaderLocation::scratchtarget2;
    bindings[16].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[16].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[16].descriptorCount = 1;
    bindings[17].binding = ShaderLocation::lumabuf;
    bindings[17].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[17].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[17].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.framesetlayout = create_descriptorsetlayout(context.vulkan, createinfo);
  }

  if (context.pipelinelayout == 0)
  {
    // PipelineLayout

    VkPushConstantRange constants[1] = {};
    constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    constants[0].offset = 0;
    constants[0].size = 64;

    VkDescriptorSetLayout layouts[4] = {};
    layouts[0] = context.scenesetlayout;
    layouts[1] = context.materialsetlayout;
    layouts[2] = context.modelsetlayout;
    layouts[3] = context.framesetlayout;

    VkPipelineLayoutCreateInfo pipelinelayoutinfo = {};
    pipelinelayoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelinelayoutinfo.pushConstantRangeCount = extentof(constants);
    pipelinelayoutinfo.pPushConstantRanges = constants;
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

    context.shadowpass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.prepass == 0)
  {
    //
    // Pre Pass
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

    context.prepass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.geometrypass == 0)
  {
    //
    // Geometry Pass
    //

    VkAttachmentDescription attachments[4] = {};
    attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
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
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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

    VkAttachmentDescription attachments[4] = {};
    attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].format = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].format = VK_FORMAT_D32_SFLOAT;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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

    VkAttachmentReference depthinputattachment = {};
    depthinputattachment.attachment = 3;
    depthinputattachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkSubpassDescription subpasses[2] = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = 1;
    subpasses[0].pColorAttachments = &colorreference[0];
    subpasses[0].inputAttachmentCount = extentof(depthinputattachment);
    subpasses[0].pInputAttachments = &depthinputattachment;
    subpasses[0].pDepthStencilAttachment = &depthinputattachment;
    subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[1].colorAttachmentCount = 2;
    subpasses[1].pColorAttachments = &colorreference[1];
    subpasses[1].pDepthStencilAttachment = &depthreference;

    VkSubpassDependency dependencies[2] = {};
    dependencies[0].srcSubpass = 0;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;
    renderpassinfo.dependencyCount = extentof(dependencies);
    renderpassinfo.pDependencies = dependencies;

    context.forwardpass = create_renderpass(context.vulkan, renderpassinfo);

    context.forwardcommands[0] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    context.forwardcommands[1] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
  }

  if (context.overlaypass == 0)
  {
    //
    // Overlay Pass
    //

    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = VK_FORMAT_B8G8R8A8_SRGB;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
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

    context.overlaypass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.sceneset == 0)
  {
    context.sceneset = create_storagebuffer(context.vulkan, SceneBufferSize);

    context.scenedescriptor = allocate_descriptorset(context.vulkan, context.descriptorpool, context.scenesetlayout);
  }

  if (context.clusterpipeline == 0)
  {
    //
    // Cluster Pipeline
    //

    auto cs = assets.find(CoreAsset::cluster_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

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

    context.clusterpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.modelshadowpipeline == 0)
  {
    //
    // Model Shadow Pipeline
    //

    auto vs = assets.find(CoreAsset::model_shadow_vert);
    auto gs = assets.find(CoreAsset::shadow_geom);
    auto fs = assets.find(CoreAsset::shadow_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

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

  if (context.modelprepasspipeline == 0)
  {
    //
    // Model Prepass Pipeline
    //

    auto vs = assets.find(CoreAsset::model_prepass_vert);
    auto fs = assets.find(CoreAsset::prepass_frag);

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
    pipelineinfo.renderPass = context.prepass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.modelprepasspipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.modelgeometrypipeline == 0)
  {
    //
    // Model Pipeline
    //

    auto vs = assets.find(CoreAsset::model_geometry_vert);
    auto fs = assets.find(CoreAsset::geometry_frag);

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
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_EQUAL;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

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

    context.modelgeometrypipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.actorshadowpipeline == 0)
  {
    //
    // Actor Shadow Pipeline
    //

    auto vs = assets.find(CoreAsset::actor_shadow_vert);
    auto gs = assets.find(CoreAsset::shadow_geom);
    auto fs = assets.find(CoreAsset::shadow_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

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

  if (context.actorprepasspipeline == 0)
  {
    //
    // Actor Prepass Pipeline
    //

    auto vs = assets.find(CoreAsset::actor_prepass_vert);
    auto fs = assets.find(CoreAsset::prepass_frag);

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
    pipelineinfo.renderPass = context.prepass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.actorprepasspipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.actorgeometrypipeline == 0)
  {
    //
    // Actor Pipeline
    //

    auto vs = assets.find(CoreAsset::actor_geometry_vert);
    auto fs = assets.find(CoreAsset::geometry_frag);

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
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_EQUAL;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

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

    context.actorgeometrypipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.depthmippipeline[0] == 0)
  {
    //
    // Depth Mip Pipeline
    //

    auto cs = assets.find(CoreAsset::depth_mip_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    for(size_t i = 0; i < extentof(context.depthmippipeline); ++i)
    {
      VkSpecializationMapEntry specializationmap[3] = {};
      specializationmap[0] = { ShaderLocation::SizeX, uint32_t(offsetof(ComputeConstants, DepthMipDispatch[0])), sizeof(ComputeConstants::DepthMipDispatch[0]) };
      specializationmap[1] = { ShaderLocation::SizeY, uint32_t(offsetof(ComputeConstants, DepthMipDispatch[1])), sizeof(ComputeConstants::DepthMipDispatch[1]) };
      specializationmap[2] = { ShaderLocation::DepthMipLayer, uint32_t(offsetof(ComputeConstants, Layers) + i*sizeof(uint32_t)), sizeof(uint32_t) };

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

      context.depthmippipeline[i] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
    }
  }

  if (context.lightingpipeline == 0)
  {
    //
    // Lighting Pipeline
    //

    auto cs = assets.find(CoreAsset::lighting_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[5] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, LightingSizeX), sizeof(ComputeConstants::LightingSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, LightingSizeY), sizeof(ComputeConstants::LightingSizeY) };
    specializationmap[2] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[3] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[4] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

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

    auto vs = assets.find(CoreAsset::skybox_vert);
    auto fs = assets.find(CoreAsset::skybox_frag);

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
    depthstate.depthWriteEnable = VK_FALSE;
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
    pipelineinfo.subpass = 0;
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

    context.skyboxcommands[0] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    context.skyboxcommands[1] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
  }

  if (context.ssaopipeline == 0)
  {
    //
    // SSAO Pipeline
    //

    auto cs = assets.find(CoreAsset::ssao_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

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

  if (context.translucentpipeline == 0)
  {
    //
    // Translucent Pipeline
    //

    auto vs = assets.find(CoreAsset::translucent_vert);
    auto fs = assets.find(CoreAsset::translucent_frag);

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

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

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
    pipelineinfo.subpass = 0;
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

  if (context.fogpipeline == 0)
  {
    //
    // Fog Pipeline
    //

    auto vs = assets.find(CoreAsset::fogplane_vert);
    auto fs = assets.find(CoreAsset::fogplane_frag);

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

    VkSpecializationMapEntry specializationmap[1] = {};
    specializationmap[0] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

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
    pipelineinfo.subpass = 0;
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

    auto vs = assets.find(CoreAsset::ocean_vert);
    auto fs = assets.find(CoreAsset::ocean_frag);

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

    auto vs = assets.find(CoreAsset::particle_vert);
    auto fs = assets.find(CoreAsset::particle_frag);

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

    VkSpecializationMapEntry specializationmap[2] = {};
    specializationmap[0] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[1] = { ShaderLocation::SoftParticles, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };

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
    pipelineinfo.subpass = 0;
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

    auto vs = assets.find(CoreAsset::water_vert);
    auto fs = assets.find(CoreAsset::water_frag);

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

    VkSpecializationMapEntry specializationmap[1] = {};
    specializationmap[0] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

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
    pipelineinfo.subpass = 0;
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

    auto cs = assets.find(CoreAsset::ssr_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

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

    auto cs = assets.find(CoreAsset::luminance_comp);

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

    context.luminancepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.bloompipeline[0] == 0)
  {
    //
    // Bloom Luma Pipeline
    //

    auto cs = assets.find(CoreAsset::bloom_luma_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

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

    auto cs = assets.find(CoreAsset::bloom_hblur_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

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

    auto cs = assets.find(CoreAsset::bloom_vblur_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

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

    auto vs = assets.find(CoreAsset::composite_vert);
    auto fs = assets.find(CoreAsset::composite_frag);

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
    pipelineinfo.renderPass = context.overlaypass;
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

    context.compositecommands[0] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    context.compositecommands[1] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
  }

  if (context.spritepipeline == 0)
  {
    //
    // Sprite Pipeline
    //

    auto vs = assets.find(CoreAsset::sprite_vert);
    auto fs = assets.find(CoreAsset::sprite_frag);

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
    pipelineinfo.renderPass = context.overlaypass;
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

    auto vs = assets.find(CoreAsset::gizmo_vert);
    auto fs = assets.find(CoreAsset::gizmo_frag);

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
    pipelineinfo.renderPass = context.overlaypass;
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

    auto vs = assets.find(CoreAsset::wireframe_vert);
    auto gs = assets.find(CoreAsset::wireframe_geom);
    auto fs = assets.find(CoreAsset::wireframe_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

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
    pipelineinfo.renderPass = context.overlaypass;
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

    auto vs = assets.find(CoreAsset::stencilmask_vert);
    auto fs = assets.find(CoreAsset::stencilmask_frag);

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
    pipelineinfo.renderPass = context.overlaypass;
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

    auto vs = assets.find(CoreAsset::stencilfill_vert);
    auto fs = assets.find(CoreAsset::stencilfill_frag);

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
    pipelineinfo.renderPass = context.overlaypass;
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

    auto vs = assets.find(CoreAsset::stencilpath_vert);
    auto gs = assets.find(CoreAsset::stencilpath_geom);
    auto fs = assets.find(CoreAsset::stencilpath_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

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
    pipelineinfo.renderPass = context.overlaypass;
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

    auto vs = assets.find(CoreAsset::line_vert);
    auto gs = assets.find(CoreAsset::line_geom);
    auto fs = assets.find(CoreAsset::line_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

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
    pipelineinfo.renderPass = context.overlaypass;
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

    auto vs = assets.find(CoreAsset::outline_vert);
    auto gs = assets.find(CoreAsset::outline_geom);
    auto fs = assets.find(CoreAsset::outline_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

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
    pipelineinfo.renderPass = context.overlaypass;
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
    auto image = assets.find(CoreAsset::envbrdf_lut);

    if (!image)
      return false;

    asset_guard lock(assets);

    auto bits = assets.request(platform, image);

    if (!bits)
      return false;

    context.envbrdf = create_texture(context.vulkan, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, bits, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
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

    context.whitediffuse = create_texture(context.vulkan, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, bits);
  }

  if (context.nominalnormal == 0)
  {
    auto image = assets.find(CoreAsset::nominal_normal);

    if (!image)
      return false;

    asset_guard lock(assets);

    auto bits = assets.request(platform, image);

    if (!bits)
      return false;

    context.nominalnormal = create_texture(context.vulkan, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, bits);
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

    context.unitquad = create_vertexbuffer(context.vulkan, context.transferbuffer, bits, mesh->vertexcount, sizeof(PackVertex));
  }

  context.width = 0;
  context.height = 0;

  context.prepared = true;

  return true;
}


///////////////////////// prepare_render_pipeline /////////////////////////
void prepare_render_pipeline(RenderContext &context, RenderParams const &params)
{
  assert(context.prepared);

  if (context.width != params.width || context.height != params.height || context.scale != params.scale || context.aspect != params.aspect || context.ssaoscale != params.ssaoscale)
  {
    context.ready = false;

    vkDeviceWaitIdle(context.vulkan);
    vkResetCommandBuffer(context.commandbuffers[0], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    vkResetCommandBuffer(context.commandbuffers[1], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    CommandPool setuppool = create_commandpool(context.vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    CommandBuffer setupbuffer = allocate_commandbuffer(context.vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    begin(context.vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (context.width != params.width || context.height != params.height || context.scale != params.scale || context.aspect != params.aspect)
    {
      release_render_pipeline(context);

      context.width = params.width;
      context.height = params.height;
      context.scale = params.scale;
      context.aspect = params.aspect;

      context.fbox = (context.width - min((int)(context.height * params.aspect), context.width))/2;
      context.fboy = (context.height - min((int)(context.width / params.aspect), context.height))/2;
      context.fbowidth = (int)(2*(context.width/2 - context.fbox) * context.scale);
      context.fboheight = (int)(2*(context.height/2 - context.fboy) * context.scale);

      assert(context.fbowidth != 0 && context.fboheight != 0);

      //
      // Shadow Map
      //

      context.shadows.shadowmap = create_texture(context.vulkan, setupbuffer, context.shadows.width, context.shadows.height, context.shadows.nslices, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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
      // Shadow Frame Buffer
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

      context.colorbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

      //
      // Geometry Attachment
      //

      context.diffusebuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      context.specularbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      context.normalbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

      //
      // Depth Attachment
      //

      context.depthbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

      VkSamplerCreateInfo depthsamplerinfo = {};
      depthsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      depthsamplerinfo.magFilter = VK_FILTER_LINEAR;
      depthsamplerinfo.minFilter = VK_FILTER_LINEAR;
      depthsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
      depthsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
      depthsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
      depthsamplerinfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

      context.depthbuffer.sampler = create_sampler(context.vulkan, depthsamplerinfo);

      // Depth Hi-Z

      uint32_t depthlayers = 6;

      context.depthmipbuffer = create_texture(context.vulkan, setupbuffer, ((context.fbowidth >> depthlayers) + 1) << (depthlayers-1), ((context.fboheight >> depthlayers) + 1) << (depthlayers-1), 1, depthlayers, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      for(uint32_t i = 0; i < depthlayers; ++i)
      {
        assert(i < extentof(context.depthmipviews));

        VkImageViewCreateInfo viewinfo = {};
        viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewinfo.format = VK_FORMAT_R16G16_SFLOAT;
        viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1};
        viewinfo.image = context.depthmipbuffer.image;

        context.depthmipviews[i] = create_imageview(context.vulkan, viewinfo);
      }

      //
      // Prepass Frame Buffer
      //

      VkImageView prebuffer[1] = {};
      prebuffer[0] = context.depthbuffer.imageview;

      VkFramebufferCreateInfo prebufferinfo = {};
      prebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      prebufferinfo.renderPass = context.prepass;
      prebufferinfo.attachmentCount = extentof(prebuffer);
      prebufferinfo.pAttachments = prebuffer;
      prebufferinfo.width = context.fbowidth;
      prebufferinfo.height = context.fboheight;
      prebufferinfo.layers = 1;

      context.preframebuffer = create_framebuffer(context.vulkan, prebufferinfo);

      //
      // Geometry Frame Buffer
      //

      VkImageView geometrybuffer[4] = {};
      geometrybuffer[0] = context.diffusebuffer.imageview;
      geometrybuffer[1] = context.specularbuffer.imageview;
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

      context.geometryframebuffer = create_framebuffer(context.vulkan, geometrybufferinfo);

      //
      // Forward Frame Buffer
      //

      VkImageView forwardbuffer[4] = {};
      forwardbuffer[0] = context.colorbuffer.imageview;
      forwardbuffer[1] = context.specularbuffer.imageview;
      forwardbuffer[2] = context.normalbuffer.imageview;
      forwardbuffer[3] = context.depthbuffer.imageview;

      VkFramebufferCreateInfo forwardbufferinfo = {};
      forwardbufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      forwardbufferinfo.renderPass = context.forwardpass;
      forwardbufferinfo.attachmentCount = extentof(forwardbuffer);
      forwardbufferinfo.pAttachments = forwardbuffer;
      forwardbufferinfo.width = context.fbowidth;
      forwardbufferinfo.height = context.fboheight;
      forwardbufferinfo.layers = 1;

      context.forwardframebuffer = create_framebuffer(context.vulkan, forwardbufferinfo);

      //
      // Render Target
      //

      context.depthstencil = create_texture(context.vulkan, setupbuffer, context.width, context.height, 1, 1, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

      //
      // Scratch Buffers
      //

      context.scratchbuffers[0] = create_texture(context.vulkan, setupbuffer, context.fbowidth/2, context.fboheight/2, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      context.scratchbuffers[1] = create_texture(context.vulkan, setupbuffer, context.fbowidth/2, context.fboheight/2, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      context.scratchbuffers[2] = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

      for(size_t i = 0; i < extentof(context.scratchbuffers); ++i)
      {
        clear(setupbuffer, context.scratchbuffers[i].image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Color4(0.0, 0.0, 0.0, 0.0));
      }
    }

    //
    // SSAO
    //

    for(size_t i = 0; i < extentof(context.ssaobuffers); ++i)
    {
      context.ssaobuffers[i] = create_texture(context.vulkan, setupbuffer, max(int(context.fbowidth*params.ssaoscale), 1), max(int(context.fboheight*params.ssaoscale), 1), 1, 1, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

      clear(setupbuffer, context.ssaobuffers[i].image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Color4(1.0, 1.0, 1.0, 1.0));
    }

    context.ssaoscale = params.ssaoscale;

    //
    // Scene Descriptor
    //

    bind_buffer(context.vulkan, context.scenedescriptor, ShaderLocation::scenebuf, context.sceneset, 0, context.sceneset.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::colormap, context.colorbuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::diffusemap, context.diffusebuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::specularmap, context.specularbuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::normalmap, context.normalbuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::depthmap, context.depthbuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::depthmipmap, context.depthmipbuffer.imageview, context.depthmipbuffer.sampler, VK_IMAGE_LAYOUT_GENERAL);

    //
    // Frame Descriptor
    //

    for(size_t i = 0; i < extentof(context.framedescriptors); ++i)
    {
      context.framedescriptors[i] = {};
      context.framedescriptors[i] = allocate_descriptorset(context.vulkan, context.descriptorpool, context.framesetlayout);

      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::colortarget, context.colorbuffer);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[0], VK_IMAGE_LAYOUT_GENERAL, 0);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[1], VK_IMAGE_LAYOUT_GENERAL, 1);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[2], VK_IMAGE_LAYOUT_GENERAL, 2);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[3], VK_IMAGE_LAYOUT_GENERAL, 3);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[4], VK_IMAGE_LAYOUT_GENERAL, 4);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[5], VK_IMAGE_LAYOUT_GENERAL, 5);
      bind_attachment(context.vulkan, context.framedescriptors[i], ShaderLocation::depthattachment, context.depthbuffer.imageview, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaomap, context.ssaobuffers[i]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::shadowmap, context.shadows.shadowmap);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::envbrdf, context.envbrdf);

      bind_buffer(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaobuf, context.ssaoset, 0, sizeof(SSAOSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaoprevmap, context.ssaobuffers[i ^ 1]);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaotarget, context.ssaobuffers[i]);

      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchmap0, context.scratchbuffers[0]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchmap1, context.scratchbuffers[1]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchmap2, context.scratchbuffers[2]);

      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchtarget0, context.scratchbuffers[0]);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchtarget1, context.scratchbuffers[1]);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchtarget2, context.scratchbuffers[2]);

      bind_buffer(context.vulkan, context.framedescriptors[i], ShaderLocation::lumabuf, context.transferbuffer, 0, sizeof(LumaSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

#if 1 // Shut up the validation layer on unused maps
    VkDescriptorImageInfo envmapinfos[8] = {};

    auto tmp = create_texture(context.vulkan, setupbuffer, 1, 1, 6, 1, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    for(size_t i = 0; i < extentof(envmapinfos); ++i)
    {
      envmapinfos[i].sampler = tmp.sampler;
      envmapinfos[i].imageView = tmp.imageview;
      envmapinfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    bind_texture(context.vulkan, context.framedescriptors[0], ShaderLocation::envmaps, envmapinfos, extentof(envmapinfos));
    bind_texture(context.vulkan, context.framedescriptors[1], ShaderLocation::envmaps, envmapinfos, extentof(envmapinfos));

    tmp.image.release();
    tmp.sampler.release();
    tmp.imageview.release();

    tmp = create_texture(context.vulkan, setupbuffer, 1, 1, 1, 1, VK_FORMAT_R32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorImageInfo spotmapinfos[16] = {};
    for(size_t i = 0; i < extentof(spotmapinfos); ++i)
    {
      spotmapinfos[i].sampler = tmp.sampler;
      spotmapinfos[i].imageView = tmp.imageview;
      spotmapinfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    bind_texture(context.vulkan, context.framedescriptors[0], ShaderLocation::spotmaps, spotmapinfos, extentof(spotmapinfos));
    bind_texture(context.vulkan, context.framedescriptors[1], ShaderLocation::spotmaps, spotmapinfos, extentof(spotmapinfos));

    tmp.image.release();
    tmp.sampler.release();
    tmp.imageview.release();
#endif

    // Finalise

    end(context.vulkan, setupbuffer);

    submit(context.vulkan, setupbuffer);

    vkQueueWaitIdle(context.vulkan.queue);

    context.ready = true;
  }
}


///////////////////////// release_render_pipeline ///////////////////////////
void release_render_pipeline(RenderContext &context)
{
  vkDeviceWaitIdle(context.vulkan);
  vkResetCommandBuffer(context.commandbuffers[0], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  vkResetCommandBuffer(context.commandbuffers[1], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

  context.ready = false;

  context.shadowbuffer = {};

  context.depthstencil = {};

  context.colorbuffer = {};
  context.diffusebuffer = {};
  context.specularbuffer = {};
  context.normalbuffer = {};
  context.depthbuffer = {};
  context.depthstencil = {};

  context.preframebuffer = {};
  context.geometryframebuffer = {};
  context.forwardframebuffer = {};

  context.frameimages[0] = {};
  context.frameimages[1] = {};
  context.frameimages[2] = {};

  context.frameviews[0] = {};
  context.frameviews[1] = {};
  context.frameviews[2] = {};

  context.framebuffers[0] = {};
  context.framebuffers[1] = {};
  context.framebuffers[2] = {};

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
  sceneset.camera.frame = context.frame;

  sceneset.mainlight.direction = params.sundirection;
  sceneset.mainlight.intensity = params.sunintensity;

  assert(sizeof(context.shadows.shadowview) <= sizeof(sceneset.mainlight.shadowview));
  memcpy(sceneset.mainlight.splits, context.shadows.splits.data(), sizeof(context.shadows.splits));
  memcpy(sceneset.mainlight.shadowview, context.shadows.shadowview.data(), sizeof(context.shadows.shadowview));

  sceneset.environmentcount = 0;
  sceneset.pointlightcount = 0;
  sceneset.spotlightcount = 0;

  VkDescriptorImageInfo envmapinfos[8] = {};
  VkDescriptorImageInfo spotmapinfos[16] = {};

  for(auto &renderable : renderables)
  {
    if (renderable.type == Renderable::Type::Lights)
    {
      auto lights = renderable_cast<Renderable::Lights>(&renderable)->lightlist;

      for(size_t i = 0; lights && i < lights->pointlightcount && sceneset.pointlightcount < extentof(sceneset.pointlights); ++i)
      {
        sceneset.pointlights[sceneset.pointlightcount].position = lights->pointlights[i].position;
        sceneset.pointlights[sceneset.pointlightcount].intensity = lights->pointlights[i].intensity;
        sceneset.pointlights[sceneset.pointlightcount].attenuation = lights->pointlights[i].attenuation;
        sceneset.pointlights[sceneset.pointlightcount].attenuation.w = params.lightfalloff * lights->pointlights[i].attenuation.w;

        ++sceneset.pointlightcount;
      }

      for(size_t i = 0; lights && i < lights->spotlightcount && sceneset.spotlightcount < extentof(sceneset.spotlights); ++i)
      {
        sceneset.spotlights[sceneset.spotlightcount].position = lights->spotlights[i].position;
        sceneset.spotlights[sceneset.spotlightcount].intensity = lights->spotlights[i].intensity;
        sceneset.spotlights[sceneset.spotlightcount].attenuation = lights->spotlights[i].attenuation;
        sceneset.spotlights[sceneset.spotlightcount].attenuation.w = params.lightfalloff * lights->spotlights[i].attenuation.w;
        sceneset.spotlights[sceneset.spotlightcount].direction = lights->spotlights[i].direction;
        sceneset.spotlights[sceneset.spotlightcount].cutoff = lights->spotlights[i].cutoff;
        sceneset.spotlights[sceneset.spotlightcount].shadowview = inverse(lights->spotlights[i].shadowview);

        spotmapinfos[sceneset.spotlightcount].sampler = lights->spotlights[i].shadowmap->texture.sampler;
        spotmapinfos[sceneset.spotlightcount].imageView = lights->spotlights[i].shadowmap->texture.imageview;
        spotmapinfos[sceneset.spotlightcount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        ++sceneset.spotlightcount;
      }

      for(size_t i = 0; lights && i < lights->environmentcount && sceneset.environmentcount + 1 < extentof(sceneset.environments); ++i)
      {
        assert(lights->environments[i].envmap && lights->environments[i].envmap->ready());

        envmapinfos[sceneset.environmentcount].sampler = lights->environments[i].envmap->texture.sampler;
        envmapinfos[sceneset.environmentcount].imageView = lights->environments[i].envmap->texture.imageview;
        envmapinfos[sceneset.environmentcount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        sceneset.environments[sceneset.environmentcount].halfdim = lights->environments[i].dimension/2;
        sceneset.environments[sceneset.environmentcount].invtransform = inverse(lights->environments[i].transform);

        ++sceneset.environmentcount;
      }
    }
  }

  if (params.skybox)
  {
    assert(params.skybox->ready());

    envmapinfos[sceneset.environmentcount].sampler = params.skybox->texture.sampler;
    envmapinfos[sceneset.environmentcount].imageView = params.skybox->texture.imageview;
    envmapinfos[sceneset.environmentcount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    sceneset.environments[sceneset.environmentcount].halfdim = Vec3(1e5f, 1e5f, 1e5f);
    sceneset.environments[sceneset.environmentcount].invtransform = inverse(params.skyboxorientation);

    sceneset.environmentcount += 1;
  }

  if (sceneset.spotlightcount != 0)
  {
    bind_texture(context.vulkan, context.framedescriptors[context.frame & 1], ShaderLocation::spotmaps, spotmapinfos, sceneset.spotlightcount);
  }

  if (sceneset.environmentcount != 0)
  {
    bind_texture(context.vulkan, context.framedescriptors[context.frame & 1], ShaderLocation::envmaps, envmapinfos, sceneset.environmentcount);
  }

  update(context.commandbuffers[context.frame & 1], context.sceneset, 0, sizeof(SceneSet), &sceneset);
}


///////////////////////// prepare_framebuffer ///////////////////////////////
void prepare_framebuffer(RenderContext &context, DatumPlatform::Viewport const &viewport)
{
  auto &frameimage = context.frameimages[context.frame & 1];
  auto &frameimageview = context.frameviews[context.frame & 1];
  auto &framebuffer = context.framebuffers[context.frame & 1];

  if (frameimage != viewport.image)
  {
    swap(frameimage, context.frameimages[2]);
    swap(frameimageview, context.frameviews[2]);
    swap(framebuffer, context.framebuffers[2]);

    if (frameimage != viewport.image)
    {
      framebuffer = {};

      if (viewport.image)
      {
        VkImageViewCreateInfo frameviewinfo = {};
        frameviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        frameviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        frameviewinfo.format = VK_FORMAT_B8G8R8A8_SRGB;
        frameviewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        frameviewinfo.image = viewport.image;

        frameimageview = create_imageview(context.vulkan, frameviewinfo);

        VkImageView frameimages[2] = {};
        frameimages[0] = frameimageview;
        frameimages[1] = context.depthstencil.imageview;

        VkFramebufferCreateInfo framebufferinfo = {};
        framebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferinfo.renderPass = context.overlaypass;
        framebufferinfo.attachmentCount = extentof(frameimages);
        framebufferinfo.pAttachments = frameimages;
        framebufferinfo.width = context.width;
        framebufferinfo.height = context.height;
        framebufferinfo.layers = 1;

        framebuffer = create_framebuffer(context.vulkan, framebufferinfo);
      }

      frameimage = viewport.image;
    }
  }
}


///////////////////////// draw_calls ////////////////////////////////////////
extern void draw_prepass(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Geometry const &geometry);
extern void draw_geometry(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Geometry const &geometry);
extern void draw_forward(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Forward const &forward);
extern void draw_casters(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Casters const &casters);
extern void draw_sprites(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Sprites const &sprites);
extern void draw_overlays(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Overlays const &overlays);

///////////////////////// render_fallback ///////////////////////////////////
void render_fallback(RenderContext &context, DatumPlatform::Viewport const &viewport, void *bitmap, int width, int height)
{
  assert(context.vulkan);

  wait_fence(context.vulkan, context.framefence);

  CommandBuffer &commandbuffer = context.commandbuffers[context.frame & 1];

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  setimagelayout(commandbuffer, viewport.image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

  clear(commandbuffer, viewport.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Color4(0.0f, 0.0f, 0.0f, 1.0f));

  if (bitmap)
  {
    size_t size = width * height * sizeof(uint32_t);

    assert(size < TransferBufferSize);

    memcpy(map_memory<uint8_t>(context.vulkan, context.transferbuffer, 0, size), bitmap, size);

    blit(commandbuffer, context.transferbuffer, 0, width, height, viewport.image, max(viewport.width - width, 0)/2, max(viewport.height - height, 0)/2, width, height);
  }

  setimagelayout(commandbuffer, viewport.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

  end(context.vulkan, commandbuffer);

  submit(context.vulkan, commandbuffer, &viewport.acquirecomplete, 1, viewport.rendercomplete, context.framefence);

  vkQueueWaitIdle(context.vulkan.queue);

  context.luminance = 1.0f;

  ++context.frame;
}


///////////////////////// render ////////////////////////////////////////////
void render(RenderContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params, VkSemaphore const (&dependancies)[8])
{
  assert(context.ready);

  context.camera = camera;
  context.proj = camera.proj();
  context.view = camera.view();

  prepare_framebuffer(context, viewport);

  auto &framebuffer = context.framebuffers[context.frame & 1];

  auto &commandbuffer = context.commandbuffers[context.frame & 1];

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  reset_querypool(commandbuffer, context.timingquerypool, 0, 1);

  prepare_shadowview(context.shadows, camera, params.sundirection);

  prepare_sceneset(context, renderables, params);

  auto &scenedescriptor = context.scenedescriptor;
  bind_descriptor(commandbuffer, context.pipelinelayout, ShaderLocation::sceneset, scenedescriptor, VK_PIPELINE_BIND_POINT_COMPUTE);

  auto &framedescriptor = context.framedescriptors[context.frame & 1];
  bind_descriptor(commandbuffer, context.pipelinelayout, ShaderLocation::frameset, framedescriptor, VK_PIPELINE_BIND_POINT_COMPUTE);

  auto &skyboxcommands = context.skyboxcommands[context.frame & 1];
  begin(context.vulkan, skyboxcommands, context.forwardframebuffer, context.forwardpass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
  bind_pipeline(skyboxcommands, context.skyboxpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_descriptor(skyboxcommands, context.pipelinelayout, ShaderLocation::sceneset, scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_descriptor(skyboxcommands, context.pipelinelayout, ShaderLocation::frameset, framedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_vertexbuffer(skyboxcommands, 0, context.unitquad);
  draw(skyboxcommands, context.unitquad.vertexcount, 1, 0, 0);
  end(context.vulkan, skyboxcommands);

  auto &compositecommands = context.compositecommands[context.frame & 1];
  begin(context.vulkan, compositecommands, framebuffer, context.overlaypass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
  bind_pipeline(compositecommands, context.compositepipeline, context.fbox, context.fboy, context.width-2*context.fbox, context.height-2*context.fboy, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_descriptor(compositecommands, context.pipelinelayout, ShaderLocation::sceneset, scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_descriptor(compositecommands, context.pipelinelayout, ShaderLocation::frameset, framedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_vertexbuffer(compositecommands, 0, context.unitquad);
  draw(compositecommands, context.unitquad.vertexcount, 1, 0, 0);
  end(context.vulkan, compositecommands);

  VkClearValue clearvalues[4];
  clearvalues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
  clearvalues[1].color = { 0.0f, 0.0f, 0.0f, 0.0f };
  clearvalues[2].color = { 0.0f, 0.0f, 0.0f, 0.0f };
  clearvalues[3].depthStencil = { 1, 0 };

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
  // Prepass
  //

  beginpass(commandbuffer, context.prepass, context.preframebuffer, 0, 0, context.fbowidth, context.fboheight, 1, &clearvalues[3]);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Geometry:
        draw_prepass(context, commandbuffer, *renderable_cast<Renderable::Geometry>(&renderable));
        break;

      default:
        break;
    }
  }

  endpass(commandbuffer, context.prepass);

  bind_pipeline(commandbuffer, context.depthmippipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, context.fbowidth/2, context.fboheight/2, 1, computeconstants.DepthMipDispatch);

  querytimestamp(commandbuffer, context.timingquerypool, 2);

  //
  // Geometry
  //

  beginpass(commandbuffer, context.geometrypass, context.geometryframebuffer, 0, 0, context.fbowidth, context.fboheight, 4, &clearvalues[0]);

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

  querytimestamp(commandbuffer, context.timingquerypool, 3);

  //
  // Cluster
  //

  bind_pipeline(commandbuffer, context.clusterpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, context.fbowidth, context.fboheight, 1, computeconstants.ClusterDispatch);

  querytimestamp(commandbuffer, context.timingquerypool, 4);

  //
  // Lighting
  //

  if (params.ssaoscale != 0)
  {
    // SSAO

    bind_pipeline(commandbuffer, context.ssaopipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.ssaobuffers[context.frame & 1], computeconstants.SSAODispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 5);

  barrier(commandbuffer, context.sceneset, 0, context.sceneset.size);

  bind_pipeline(commandbuffer, context.lightingpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, context.colorbuffer, computeconstants.LightingDispatch);

  querytimestamp(commandbuffer, context.timingquerypool, 6);

  //
  // Forward
  //

  auto &forwardcommands = context.forwardcommands[context.frame & 1];

  begin(context.vulkan, forwardcommands, context.forwardframebuffer, context.forwardpass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
  bind_descriptor(forwardcommands, context.pipelinelayout, ShaderLocation::sceneset, scenedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_descriptor(forwardcommands, context.pipelinelayout, ShaderLocation::frameset, framedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Forward:
        draw_forward(context, forwardcommands, *renderable_cast<Renderable::Forward>(&renderable));
        break;

      default:
        break;
    }
  }

  end(context.vulkan, forwardcommands);

  beginpass(commandbuffer, context.forwardpass, context.forwardframebuffer, 0, 0, context.fbowidth, context.fboheight, 0, nullptr);

  if (params.skybox)
  {
    execute(commandbuffer, skyboxcommands);
  }

  execute(commandbuffer, forwardcommands);

  nextsubpass(commandbuffer, context.forwardpass);

  endpass(commandbuffer, context.forwardpass);

  querytimestamp(commandbuffer, context.timingquerypool, 7);

  //
  // SSR
  //

  if (params.ssrstrength != 0)
  {
    for(int i = 0; i < 6; ++i)
    {
      bind_pipeline(commandbuffer, context.depthmippipeline[i], VK_PIPELINE_BIND_POINT_COMPUTE);

      dispatch(commandbuffer, context.depthmipbuffer.width >> i, context.depthmipbuffer.height >> i, 1, computeconstants.DepthMipDispatch);

      setimagelayout(commandbuffer, context.depthmipbuffer.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    }

    bind_pipeline(commandbuffer, context.ssrpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.scratchbuffers[2], computeconstants.SSRDispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 8);

  //
  // Luminance
  //

  bind_pipeline(commandbuffer, context.luminancepipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, 1, 1, 1);

  querytimestamp(commandbuffer, context.timingquerypool, 9);

  //
  // Bloom
  //

  if (params.bloomstrength != 0)
  {
    bind_pipeline(commandbuffer, context.bloompipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.scratchbuffers[0], computeconstants.BloomLumaDispatch);

    bind_pipeline(commandbuffer, context.bloompipeline[1], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.scratchbuffers[1], computeconstants.BloomHBlurDispatch);

    bind_pipeline(commandbuffer, context.bloompipeline[2], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.scratchbuffers[0], computeconstants.BloomVBlurDispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 10);

  //
  // Overlay
  //

  if (viewport.image)
  {
    transition_acquire(commandbuffer, viewport.image);

    beginpass(commandbuffer, context.overlaypass, framebuffer, 0, 0, context.width, context.height, 2, &clearvalues[2]);

    execute(commandbuffer, compositecommands);

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

    endpass(commandbuffer, context.overlaypass);

    transition_present(commandbuffer, viewport.image);

    querytimestamp(commandbuffer, context.timingquerypool, 11);
  }

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
  retreive_querypool(context.vulkan, context.timingquerypool, 0, 12, timings);

  GPU_TIMED_BLOCK(Shadows, Color3(0.0f, 0.4f, 0.0f), timings[0], timings[1])
  GPU_TIMED_BLOCK(PrePass, Color3(0.4f, 0.2f, 0.4f), timings[1], timings[2])
  GPU_TIMED_BLOCK(Geometry, Color3(0.4f, 0.0f, 0.4f), timings[2], timings[3])
  GPU_TIMED_BLOCK(Cluster, Color3(0.5f, 0.5f, 0.1f), timings[3], timings[4])
  GPU_TIMED_BLOCK(SSAO, Color3(0.2f, 0.8f, 0.2f), timings[4], timings[5])
  GPU_TIMED_BLOCK(Lighting, Color3(0.0f, 0.6f, 0.4f), timings[5], timings[6])
  GPU_TIMED_BLOCK(Forward, Color3(0.2f, 0.3f, 0.6f), timings[6], timings[7])
  GPU_TIMED_BLOCK(SSR, Color3(0.0f, 0.4f, 0.8f), timings[7], timings[8])
  GPU_TIMED_BLOCK(Luminance, Color3(0.8f, 0.4f, 0.2f), timings[8], timings[9])
  GPU_TIMED_BLOCK(Bloom, Color3(0.5f, 0.2f, 0.6f), timings[9], timings[10])
  GPU_TIMED_BLOCK(Overlay, Color3(0.4f, 0.4f, 0.0f), timings[10], timings[11])

  GPU_SUBMIT();

  int waitsemaphorescount = 0;
  VkSemaphore waitsemaphores[8 + 1];

  for(auto &dependancy : dependancies)
  {
    if (dependancy)
    {
      waitsemaphores[waitsemaphorescount++] = dependancy;
    }
  }

  waitsemaphores[waitsemaphorescount++] = viewport.acquirecomplete;

  submit(context.vulkan, commandbuffer, waitsemaphores, waitsemaphorescount, viewport.rendercomplete, context.framefence);

  context.prevcamera = camera;

  ++context.frame;
}
