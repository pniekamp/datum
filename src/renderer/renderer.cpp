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
using namespace DatumPlatform;
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

  sceneset = 0,
  materialset = 1,
  modelset = 2,
  computeset = 3,

  rt0 = 1,
  rt1 = 2,
  normalmap = 3,
  depthmap = 4,
  ssaomap = 5,
  shadowmap = 6,
  envmaps = 7,
  envbrdf = 8,

  colorbuffer = 1,

  imagetarget = 1,

  scratchmap0 = 5,
  scratchmap1 = 6,

  // Constant Ids

  SizeX = 1,
  SizeY = 2,
  SizeZ = 3,

  ShadowSlices = 46,

  MaxPointLights = 29,

  MaxEnvironments = 31,

  MaxTileLights = 72,

  NoiseSize = 62,
  KernelSize = 63,

  SSAORadius = 67,

  BloomCutoff = 81,
  BloomBlurRadius = 84,
  BloomBlurSigma = 85,
};

static constexpr size_t PushConstantSize = 64;
static constexpr size_t ConstantBufferSize = 64*1024;
static constexpr size_t TransferBufferSize = 512*1024;

struct alignas(16) MainLight
{
  Vec4 direction;
  Color4 intensity;
};

struct alignas(16) PointLight
{
  Vec4 position;
  Color4 intensity;
  Vec4 attenuation;
};

struct alignas(16) Environment
{
  Vec4 halfdim;
  Transform invtransform;
};

struct alignas(16) CameraView
{
  Vec3 position;
  float exposure;
  float skyboxlod;
  float ssrstrength;
  float bloomstrength;
};

struct SceneSet
{
  Matrix4f proj;
  Matrix4f invproj;
  Matrix4f view;
  Matrix4f invview;
  Matrix4f prevview;
  Matrix4f skyview;

  CameraView camera;

  MainLight mainlight;

  uint32_t envcount;
  Environment environments[6];

  float splits[4];
  Matrix4f shadowview[4];

  uint32_t pointlightcount;
  PointLight pointlights[256];
};

struct ComputeSet
{
  Vec4 noise[16];
  Vec4 kernel[16];

  float luminance;
};

struct ComputeConstants
{
  uint32_t ShadowSlices = ShadowMap::nslices;
  uint32_t MaxPointLights = extent<decltype(SceneSet::pointlights)>::value;

  uint32_t MaxTileLights = 48;
  uint32_t MaxEnvironments = 6;

  uint32_t NoiseSize = extent<decltype(ComputeSet::noise)>::value;
  uint32_t KernelSize = extent<decltype(ComputeSet::kernel)>::value;

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

  uint32_t BloomToneDispatch[3] = { 4, 32, 1 };
  uint32_t BloomToneSizeX = BloomToneDispatch[0];
  uint32_t BloomToneSizeY = BloomToneDispatch[1];

} computeconstants;


//|---------------------- PushBuffer ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// PushBuffer::Constructor ///////////////////////////
PushBuffer::PushBuffer(allocator_type const &allocator, size_t slabsize)
{
  m_slabsize = slabsize;
  m_slab = allocate<char, alignof(Header)>(allocator, m_slabsize);

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

///////////////////////// prepare_render_context ////////////////////////////
bool prepare_render_context(DatumPlatform::PlatformInterface &platform, RenderContext &context, AssetManager *assets)
{
  if (context.initialised)
    return true;

  if (context.device == 0)
  {
    //
    // Vulkan Device
    //

    auto renderdevice = platform.render_device();

    initialise_vulkan_device(&context.device, renderdevice.physicaldevice, renderdevice.device, 0);

    context.framefence = create_fence(context.device, VK_FENCE_CREATE_SIGNALED_BIT);

    context.commandpool = create_commandpool(context.device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    context.commandbuffers[0] = allocate_commandbuffer(context.device, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    context.commandbuffers[1] = allocate_commandbuffer(context.device, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    assert(PushConstantSize <= context.device.physicaldeviceproperties.limits.maxPushConstantsSize);

    // DescriptorPool

    VkDescriptorPoolSize typecounts[3] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    typecounts[0].descriptorCount = 16;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[1].descriptorCount = 128;
    typecounts[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    typecounts[2].descriptorCount = 8;

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

    // Constant Buffer

    context.constantbuffer = create_constantbuffer(context.device, ConstantBufferSize);

    // Transfer Buffer

    context.transferbuffer = create_transferbuffer(context.device, TransferBufferSize);
  }

  if (context.timingquerypool == 0)
  {
    // Timing QueryPool

    VkQueryPoolCreateInfo querypoolinfo = {};
    querypoolinfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    querypoolinfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    querypoolinfo.queryCount = 16;

    context.timingquerypool = create_querypool(context.device, querypoolinfo);
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

    VkDescriptorSetLayoutBinding bindings[9] = {};
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
    bindings[7].descriptorCount = 6;
    bindings[8].binding = 8;
    bindings[8].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[8].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.scenesetlayout = create_descriptorsetlayout(context.device, createinfo);
  }

  if (context.materialsetlayout == 0)
  {
    // Material Set

    VkDescriptorSetLayoutBinding bindings[4] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
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

    context.materialsetlayout = create_descriptorsetlayout(context.device, createinfo);
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

    context.modelsetlayout = create_descriptorsetlayout(context.device, createinfo);
  }

  if (context.computelayout == 0)
  {
    // Compute Set

    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.computelayout = create_descriptorsetlayout(context.device, createinfo);
  }

  if (context.pipelinelayout == 0)
  {
    // PipelineLayout

    VkPushConstantRange constants[1] = {};
    constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    constants[0].offset = 0;
    constants[0].size = PushConstantSize;

    VkDescriptorSetLayout layouts[4] = {};
    layouts[0] = context.scenesetlayout;
    layouts[1] = context.materialsetlayout;
    layouts[2] = context.modelsetlayout;
    layouts[3] = context.computelayout;

    VkPipelineLayoutCreateInfo pipelinelayoutinfo = {};
    pipelinelayoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelinelayoutinfo.pushConstantRangeCount = extentof(constants);
    pipelinelayoutinfo.pPushConstantRanges = constants;
    pipelinelayoutinfo.setLayoutCount = extentof(layouts);
    pipelinelayoutinfo.pSetLayouts = layouts;

    context.pipelinelayout = create_pipelinelayout(context.device, pipelinelayoutinfo);
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
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

    context.shadowpass = create_renderpass(context.device, renderpassinfo);
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
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].format = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].format = VK_FORMAT_D32_SFLOAT;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

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

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;

    context.geometrypass = create_renderpass(context.device, renderpassinfo);
  }

  if (context.renderpass == 0)
  {
    //
    // Render Pass
    //

    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorreference = {};
    colorreference.attachment = 0;
    colorreference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthreference = {};
    depthreference.attachment = 1;
    depthreference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[1] = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = 1;
    subpasses[0].pColorAttachments = &colorreference;
    subpasses[0].pDepthStencilAttachment = &depthreference;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;

    context.renderpass = create_renderpass(context.device, renderpassinfo);
  }

  if (context.shadowpipeline == 0)
  {
    //
    // Shadow Pipeline
    //

    auto vs = assets->find(CoreAsset::shadow_vert);
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

    auto vsmodule = create_shadermodule(context.device, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.device, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.device, fssrc, fs->length);

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

    context.shadowpipeline = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
  }

  if (context.geometrypipeline == 0)
  {
    //
    // Geometry Pipeline
    //

    auto vs = assets->find(CoreAsset::geometry_vert);
    auto fs = assets->find(CoreAsset::geometry_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets->request(platform, vs);
    auto fssrc = assets->request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.device, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.device, fssrc, fs->length);

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

    context.geometrypipeline = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
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

    auto csmodule = create_shadermodule(context.device, cssrc, cs->length);

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

    context.ssaopipeline = create_pipeline(context.device, context.pipelinecache, pipelineinfo);

    mt19937 random(random_device{}());
    uniform_real_distribution<float> unit(0.0f, 1.0f);

    for(size_t i = 0; i < extentof(context.ssaonoise); ++i)
    {
      context.ssaonoise[i] = normalise(Vec4(2*unit(random)-1, 2*unit(random)-1, 0.0f, 0.0f));
    }

    for(size_t i = 0; i < extentof(context.ssaokernel); ++i)
    {
      context.ssaokernel[i] = normalise(Vec4(2*unit(random)-1, 2*unit(random)-1, unit(random), 0.0f)) * lerp(0.1f, 1.0f, pow(i / (float)extentof(context.ssaokernel), 2.0f));
    }

    context.ssaodescriptors[0] = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.constantbuffer, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
    context.ssaodescriptors[1] = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.constantbuffer, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
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

    auto csmodule = create_shadermodule(context.device, cssrc, cs->length);

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

    context.lightingpipeline = create_pipeline(context.device, context.pipelinecache, pipelineinfo);

    assert(sizeof(SceneSet) < ConstantBufferSize);

    context.lightingdescriptors[0] = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.constantbuffer, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
    context.lightingdescriptors[1] = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.constantbuffer, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
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

    auto vsmodule = create_shadermodule(context.device, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.device, fssrc, fs->length);

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

    context.skyboxpipeline = create_pipeline(context.device, context.pipelinecache, pipelineinfo);

    for(size_t i = 0; i < extentof(context.skyboxdescriptors); ++i)
    {
      context.skyboxcommands[i] = allocate_commandbuffer(context.device, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
      context.skyboxdescriptors[i] = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.constantbuffer, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
    }
  }

  if (context.ssrpipeline[0] == 0)
  {
    //
    // SSR Gen Pipeline
    //

    auto cs = assets->find(CoreAsset::ssr_gen_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.device, cssrc, cs->length);

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

    context.ssrpipeline[0] = create_pipeline(context.device, context.pipelinecache, pipelineinfo);

    context.ssrdescriptor = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.constantbuffer, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
  }

  if (context.ssrpipeline[1] == 0)
  {
    //
    // SSR Blend Pipeline
    //

    auto cs = assets->find(CoreAsset::ssr_blend_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.device, cssrc, cs->length);

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

    context.ssrpipeline[1] = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
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

    auto csmodule = create_shadermodule(context.device, cssrc, cs->length);

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.luminancepipeline = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
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

    auto csmodule = create_shadermodule(context.device, cssrc, cs->length);

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

    context.bloompipeline[0] = create_pipeline(context.device, context.pipelinecache, pipelineinfo);

    context.bloomdescriptor = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.constantbuffer, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
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

    auto csmodule = create_shadermodule(context.device, cssrc, cs->length);

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

    context.bloompipeline[1] = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
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

    auto csmodule = create_shadermodule(context.device, cssrc, cs->length);

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

    context.bloompipeline[2] = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
  }

  if (context.bloompipeline[3] == 0)
  {
    //
    // Bloom Tone Pipeline
    //

    auto cs = assets->find(CoreAsset::bloom_tone_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.device, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[2] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, BloomToneSizeX), sizeof(ComputeConstants::BloomToneSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, BloomToneSizeY), sizeof(ComputeConstants::BloomToneSizeY) };

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

    context.bloompipeline[3] = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
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

    auto vsmodule = create_shadermodule(context.device, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.device, fssrc, fs->length);

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
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
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

    context.spritepipeline = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
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

    context.envbrdf = create_texture(context.device, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, bits, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
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

    context.whitediffuse = create_texture(context.device, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, bits);
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

    context.nominalnormal = create_texture(context.device, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, bits);
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

    context.unitquad = create_vertexbuffer(context.device, context.transferbuffer, bits, mesh->vertexcount, sizeof(PackVertex));
  }

  context.transfermemory = map_memory<uint8_t>(context.device, context.transferbuffer, 0, context.transferbuffer.size);

  context.fbowidth = 0;
  context.fboheight = 0;

  context.initialised = true;

  return true;
}


///////////////////////// prepare_render_pipeline /////////////////////////
bool prepare_render_pipeline(RenderContext &context, RenderParams const &params)
{
  bool dirty = false;
  dirty |= (context.fbowidth != params.width || context.fboheight != params.height);
  dirty |= (context.ssaoscale != params.ssaoscale);

  if (dirty)
  {
    int width = params.width;
    int height = params.height;

    vkDeviceWaitIdle(context.device);
    vkResetCommandBuffer(context.commandbuffers[0], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    vkResetCommandBuffer(context.commandbuffers[1], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    if (width == 0 || height == 0)
      return false;

    //
    // Shadow Map
    //

    context.shadows.shadowmap = create_texture(context.device, context.shadows.width, context.shadows.height, context.shadows.nslices, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo shadowsamplerinfo = {};
    shadowsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    shadowsamplerinfo.magFilter = VK_FILTER_LINEAR;
    shadowsamplerinfo.minFilter = VK_FILTER_LINEAR;
    shadowsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowsamplerinfo.compareEnable = VK_TRUE;
    shadowsamplerinfo.compareOp = VK_COMPARE_OP_LESS;

    context.shadows.shadowmap.sampler = create_sampler(context.device, shadowsamplerinfo);

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

    context.shadowbuffer = create_framebuffer(context.device, shadowbufferinfo);

    //
    // Color Attachment
    //

    context.colorbuffer = create_texture(context.device, width, height, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    setimagelayout(context.device, context.colorbuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    context.colorbuffertarget = allocate_descriptorset(context.device, context.descriptorpool, context.computelayout, context.transferbuffer, 0, sizeof(ComputeSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);

    bindimageview(context.device, context.colorbuffertarget, ShaderLocation::imagetarget, context.colorbuffer.imageview);

    //
    // Geometry Attachment
    //

    context.rt0buffer = create_texture(context.device, width, height, 1, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    context.rt1buffer = create_texture(context.device, width, height, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    context.normalbuffer = create_texture(context.device, width, height, 1, 1, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    //
    // Depth Attachment
    //

    context.depthbuffer = create_texture(context.device, width, height, 1, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo depthsamplerinfo = {};
    depthsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    depthsamplerinfo.magFilter = VK_FILTER_LINEAR;
    depthsamplerinfo.minFilter = VK_FILTER_LINEAR;
    depthsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    depthsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    depthsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    depthsamplerinfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    context.depthbuffer.sampler = create_sampler(context.device, depthsamplerinfo);

    setimagelayout(context.device, context.depthbuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });

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
    geometrybufferinfo.width = width;
    geometrybufferinfo.height = height;
    geometrybufferinfo.layers = 1;

    context.geometrybuffer = create_framebuffer(context.device, geometrybufferinfo);

    //
    // Frame Buffer
    //

    VkImageView framebuffer[2] = {};
    framebuffer[0] = context.colorbuffer.imageview;
    framebuffer[1] = context.depthbuffer.imageview;

    VkFramebufferCreateInfo framebufferinfo = {};
    framebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferinfo.renderPass = context.renderpass;
    framebufferinfo.attachmentCount = extentof(framebuffer);
    framebufferinfo.pAttachments = framebuffer;
    framebufferinfo.width = width;
    framebufferinfo.height = height;
    framebufferinfo.layers = 1;

    context.framebuffer = create_framebuffer(context.device, framebufferinfo);

    context.fbowidth = width;
    context.fboheight = height;
    context.fbox = (context.fbowidth - min((int)(context.fboheight * params.aspect), context.fbowidth)) / 2;
    context.fboy = (context.fboheight - min((int)(context.fbowidth / params.aspect), context.fboheight)) / 2;

    //
    // Scratch Buffers
    //

    context.scratchbuffers[0] = create_texture(context.device, width/2, height/2, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    context.scratchbuffers[1] = create_texture(context.device, width/2, height/2, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    context.scratchbuffers[2] = create_texture(context.device, width, height, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    context.scratchbuffers[3] = create_texture(context.device, width, height, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

    for(size_t i = 0; i < extentof(context.scratchbuffers); ++i)
    {
      setimagelayout(context.device, context.scratchbuffers[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

      context.scratchtargets[i] = allocate_descriptorset(context.device, context.descriptorpool, context.computelayout, context.transferbuffer, 0, sizeof(ComputeSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);

      bindimageview(context.device, context.scratchtargets[i], ShaderLocation::imagetarget, context.scratchbuffers[i].imageview);
    }

    //
    // SSAO
    //

    context.ssaotargets[0] = {};
    context.ssaotargets[1] = {};
    context.ssaobuffers[0] = {};
    context.ssaobuffers[1] = {};

    for(size_t i = 0; i < extentof(context.ssaodescriptors); ++i)
    {
      context.ssaobuffers[i] = create_texture(context.device, max(int(width*params.ssaoscale), 1), max(int(height*params.ssaoscale), 1), 1, 1, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

      setimagelayout(context.device, context.ssaobuffers[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

      context.ssaotargets[i] = allocate_descriptorset(context.device, context.descriptorpool, context.computelayout, context.transferbuffer, 0, sizeof(ComputeSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);

      bindimageview(context.device, context.ssaotargets[i], ShaderLocation::imagetarget, context.ssaobuffers[i].imageview);

      bindtexture(context.device, context.ssaodescriptors[i], ShaderLocation::rt0, context.rt0buffer);
      bindtexture(context.device, context.ssaodescriptors[i], ShaderLocation::rt1, context.rt1buffer);
      bindtexture(context.device, context.ssaodescriptors[i], ShaderLocation::normalmap, context.normalbuffer);
      bindtexture(context.device, context.ssaodescriptors[i], ShaderLocation::depthmap, context.depthbuffer);

      clear(context.device, context.ssaobuffers[i].image, Color4(1.0, 1.0, 1.0, 1.0));
    }

    bindtexture(context.device, context.ssaodescriptors[0], ShaderLocation::ssaomap, context.ssaobuffers[1]);
    bindtexture(context.device, context.ssaodescriptors[1], ShaderLocation::ssaomap, context.ssaobuffers[0]);

    context.ssaoscale = params.ssaoscale;

    //
    // Lighting
    //

    for(size_t i = 0; i < extentof(context.lightingdescriptors); ++i)
    {
      bindtexture(context.device, context.lightingdescriptors[i], ShaderLocation::rt0, context.rt0buffer);
      bindtexture(context.device, context.lightingdescriptors[i], ShaderLocation::rt1, context.rt1buffer);
      bindtexture(context.device, context.lightingdescriptors[i], ShaderLocation::normalmap, context.normalbuffer);
      bindtexture(context.device, context.lightingdescriptors[i], ShaderLocation::depthmap, context.depthbuffer);
      bindtexture(context.device, context.lightingdescriptors[i], ShaderLocation::ssaomap, context.ssaobuffers[i]);
      bindtexture(context.device, context.lightingdescriptors[i], ShaderLocation::shadowmap, context.shadows.shadowmap);
      bindtexture(context.device, context.lightingdescriptors[i], ShaderLocation::envbrdf, context.envbrdf);
    }

    //
    // SSR
    //

    bindtexture(context.device, context.ssrdescriptor, ShaderLocation::colorbuffer, context.colorbuffer);
    bindtexture(context.device, context.ssrdescriptor, ShaderLocation::rt1, context.rt1buffer);
    bindtexture(context.device, context.ssrdescriptor, ShaderLocation::normalmap, context.normalbuffer);
    bindtexture(context.device, context.ssrdescriptor, ShaderLocation::depthmap, context.depthbuffer);
    bindtexture(context.device, context.ssrdescriptor, ShaderLocation::scratchmap0, context.scratchbuffers[2]);

    //
    // Bloom
    //

    bindtexture(context.device, context.bloomdescriptor, ShaderLocation::colorbuffer, context.scratchbuffers[3]);
    bindtexture(context.device, context.bloomdescriptor, ShaderLocation::scratchmap0, context.scratchbuffers[0]);
    bindtexture(context.device, context.bloomdescriptor, ShaderLocation::scratchmap1, context.scratchbuffers[1]);

    return false;
  }

  return true;
}


///////////////////////// release_render_pipeline ///////////////////////////
void release_render_pipeline(RenderContext &context)
{
  vkDeviceWaitIdle(context.device);
  vkResetCommandBuffer(context.commandbuffers[0], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  vkResetCommandBuffer(context.commandbuffers[1], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

  context.shadowbuffer = {};

  context.colorbuffer = {};

  context.rt0buffer = {};
  context.rt1buffer = {};
  context.normalbuffer = {};
  context.depthbuffer = {};

  context.geometrybuffer = {};
  context.framebuffer = {};

  context.scratchbuffers[0] = {};
  context.scratchbuffers[1] = {};
  context.scratchbuffers[2] = {};
  context.scratchbuffers[3] = {};

  context.ssaobuffers[0] = {};
  context.ssaobuffers[1] = {};

  context.fbowidth = 0;
  context.fboheight = 0;
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

    auto radius = 0.5f * norm(frustum.corners[0] - frustum.corners[6]);

    auto frustumcentre = frustum.centre();

    frustumcentre = inverse(snapview) * frustumcentre;
    frustumcentre.x -= fmod(frustumcentre.x, (radius + radius) / shadowmap.width);
    frustumcentre.y -= fmod(frustumcentre.y, (radius + radius) / shadowmap.height);
    frustumcentre = snapview * frustumcentre;

    auto lightpos = frustumcentre - extrusion * lightdirection;

    auto lightview = Transform::lookat(lightpos, lightpos + lightdirection, up);

    auto lightproj = OrthographicProjection(-radius, -radius, radius, radius, 0.1f, extrusion + radius) * ScaleMatrix(Vector4(1.0f, -1.0f, 1.0f, 1.0f));

    shadowmap.splits[i] = splits[i+1];
    shadowmap.shadowview[i] = lightproj * inverse(lightview).matrix();
  }
}


///////////////////////// prepare_sceneset //////////////////////////////////
void prepare_sceneset(RenderContext &context, SceneSet *scene, PushBuffer const &renderables, RenderParams const &params)
{
  auto viewport = ScaleMatrix(Vector4((context.fbowidth - 2.0f*context.fbox) / context.fbowidth, (context.fboheight - 2.0f*context.fboy) / context.fboheight, 1.0f, 1.0f));

  scene->proj = context.proj;
  scene->invproj = inverse(scene->proj);
  scene->view = viewport * context.view;
  scene->invview = inverse(scene->view);
  scene->prevview = viewport * context.prevcamera.view();
  scene->skyview = (inverse(params.skyboxorientation) * Transform::rotation(context.camera.rotation())).matrix() * scene->invproj;
  scene->camera.position = context.camera.position();
  scene->camera.exposure = context.camera.exposure();
  scene->camera.skyboxlod = params.skyboxlod;
  scene->camera.ssrstrength = params.ssrstrength;
  scene->camera.bloomstrength = params.bloomstrength;

  assert(sizeof(scene->shadowview) <= sizeof(context.shadows.shadowview));
  memcpy(scene->splits, context.shadows.splits.data(), sizeof(context.shadows.splits));
  memcpy(scene->shadowview, context.shadows.shadowview.data(), sizeof(context.shadows.shadowview));

  auto &mainlight = scene->mainlight;
  auto &pointlights = scene->pointlights;

  mainlight.direction.xyz = params.sundirection;
  mainlight.intensity.rgb = params.sunintensity;

  auto &pointlightcount = scene->pointlightcount = 0;

  for(auto &renderable : renderables)
  {
    if (renderable.type == Renderable::Type::Lights)
    {
      auto lights = renderable_cast<Renderable::Lights>(&renderable)->commandlist->lookup<LightList::Lights>(ShaderLocation::sceneset);

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

  auto &envcount = scene->envcount = 0;

  VkDescriptorImageInfo imageinfos[6] = {};

  for(auto &renderable : renderables)
  {
    if (renderable.type == Renderable::Type::Environment)
    {
      auto &environment = *renderable_cast<Renderable::Environment>(&renderable);

      if (environment.envmap && environment.envmap->ready() && envcount + 1 < extentof(imageinfos))
      {
        imageinfos[envcount].sampler = environment.envmap->texture.sampler;
        imageinfos[envcount].imageView = environment.envmap->texture.imageview;
        imageinfos[envcount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        scene->environments[envcount].halfdim = Vec4(environment.dimension/2, 0);
        scene->environments[envcount].invtransform = inverse(environment.transform);

        envcount += 1;
      }
    }
  }

  if (params.skybox && params.skybox->ready())
  {
    imageinfos[envcount].sampler = params.skybox->envmap->texture.sampler;
    imageinfos[envcount].imageView = params.skybox->envmap->texture.imageview;
    imageinfos[envcount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    scene->environments[envcount].halfdim = Vec4(1e5f, 1e5f, 1e5f, 0);
    scene->environments[envcount].invtransform = inverse(params.skyboxorientation);

    envcount += 1;
  }

  if (envcount != 0)
  {
    bindtexture(context.device, context.lightingdescriptors[context.frame & 1], ShaderLocation::envmaps, imageinfos, envcount);
  }
}


///////////////////////// prepare_computeset ////////////////////////////////
void prepare_computeset(RenderContext &context, ComputeSet *compute, PushBuffer const &renderables, RenderParams const &params)
{
  assert(sizeof(compute->noise) <= sizeof(compute->noise));
  memcpy(compute->noise, context.ssaonoise, sizeof(compute->noise));

  assert(sizeof(compute->kernel) <= sizeof(compute->kernel));
  memcpy(compute->kernel, context.ssaokernel, sizeof(compute->kernel));
}


///////////////////////// draw_calls ////////////////////////////////////////
extern void draw_meshes(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Meshes const &meshes);
extern void draw_casters(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Casters const &casters);
extern void draw_skybox(RenderContext &context, VkCommandBuffer commandbuffer, RenderParams const &params);
extern void draw_sprites(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Sprites const &sprites);


///////////////////////// render_fallback ///////////////////////////////////
void render_fallback(RenderContext &context, DatumPlatform::Viewport const &viewport, void *bitmap, int width, int height)
{
  CommandBuffer &commandbuffer = context.commandbuffers[context.frame & 1];

  wait(context.device, context.framefence);

  begin(context.device, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  transition_acquire(commandbuffer, viewport.image);

  clear(commandbuffer, viewport.image, Color4(0.0f, 0.0f, 0.0f, 1.0f));

  if (bitmap)
  {
    size_t size = width * height * sizeof(uint32_t);

    assert(size < TransferBufferSize);

    memcpy(context.transfermemory ? (void*)context.transfermemory : (void*)map_memory<uint8_t>(context.device, context.transferbuffer, 0, size), bitmap, size);

    blit(commandbuffer, context.transferbuffer, 0, width, height, viewport.image, max(viewport.width - width, 0)/2, max(viewport.height - height, 0)/2, width, height);
  }

  transition_present(commandbuffer, viewport.image);

  end(context.device, commandbuffer);

  submit(context.device, commandbuffer, viewport.acquirecomplete, viewport.rendercomplete, context.framefence);

  context.luminance = 1.0f;

  ++context.frame;
}


///////////////////////// render ////////////////////////////////////////////
void render(RenderContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params)
{
  if (!prepare_render_pipeline(context, params))
  {
    render_fallback(context, viewport);
    return;
  }

  context.camera = camera;
  context.proj = camera.proj();
  context.view = camera.view();

  auto &commandbuffer = context.commandbuffers[context.frame & 1];

  begin(context.device, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  reset_querypool(commandbuffer, context.timingquerypool, 0, 1);

  prepare_shadowview(context.shadows, camera, params.sundirection);

  SceneSet sceneset;
  prepare_sceneset(context, &sceneset, renderables, params);

  update(commandbuffer, context.constantbuffer, 0, sizeof(sceneset), &sceneset);

  ComputeSet computeset;
  prepare_computeset(context, &computeset, renderables, params);

  update(commandbuffer, context.transferbuffer, 0, sizeof(computeset), &computeset);

  VkClearValue clearvalues[4];
  clearvalues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
  clearvalues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };
  clearvalues[2].color = { 0.0f, 0.0f, 0.0f, 1.0f };
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
  // Geometry
  //

  beginpass(commandbuffer, context.geometrypass, context.geometrybuffer, 0, 0, context.fbowidth, context.fboheight, extentof(clearvalues), clearvalues);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Meshes:
        draw_meshes(context, commandbuffer, *renderable_cast<Renderable::Meshes>(&renderable));
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
    bindresource(commandbuffer, context.ssaopipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.ssaodescriptors[context.frame & 1], context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.ssaotargets[context.frame & 1], context.pipelinelayout, ShaderLocation::computeset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.ssaobuffers[0].width, context.ssaobuffers[0].height, 1, computeconstants.SSAODispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 3);

  bindresource(commandbuffer, context.lightingpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  bindresource(commandbuffer, context.lightingdescriptors[context.frame & 1], context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

  bindresource(commandbuffer, context.colorbuffertarget, context.pipelinelayout, ShaderLocation::computeset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, context.fbowidth, context.fboheight, 1, computeconstants.LightingDispatch);

  querytimestamp(commandbuffer, context.timingquerypool, 4);

  //
  // SkyBox
  //

  beginpass(commandbuffer, context.renderpass, context.framebuffer, 0, 0, context.fbowidth, context.fboheight, extentof(clearvalues), clearvalues);

  draw_skybox(context, commandbuffer, params);

  endpass(commandbuffer, context.renderpass);

  querytimestamp(commandbuffer, context.timingquerypool, 5);

  //
  // SSR
  //

  if (params.ssrstrength != 0)
  {
    bindresource(commandbuffer, context.ssrpipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.ssrdescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.scratchtargets[2], context.pipelinelayout, ShaderLocation::computeset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth, context.fboheight, 1, computeconstants.SSRDispatch);

    bindresource(commandbuffer, context.ssrpipeline[1], VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.scratchtargets[3], context.pipelinelayout, ShaderLocation::computeset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth, context.fboheight, 1, computeconstants.SSRDispatch);
  }
  else
  {
    blit(commandbuffer, context.colorbuffer.image, 0, 0, context.fbowidth, context.fboheight, context.scratchbuffers[3].image, 0, 0, context.fbowidth, context.fboheight, VK_FILTER_NEAREST);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 6);

  //
  // Luminance
  //

  bindresource(commandbuffer, context.luminancepipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  bindresource(commandbuffer, context.bloomdescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

  bindresource(commandbuffer, context.colorbuffertarget, context.pipelinelayout, ShaderLocation::computeset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, 1, 1, 1);

  querytimestamp(commandbuffer, context.timingquerypool, 7);

  //
  // Bloom
  //

  if (params.bloomstrength != 0)
  {
    bindresource(commandbuffer, context.bloompipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.bloomdescriptor, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.scratchtargets[0], context.pipelinelayout, ShaderLocation::computeset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth/2, context.fboheight/2, 1, computeconstants.BloomLumaDispatch);

    bindresource(commandbuffer, context.bloompipeline[1], VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.scratchtargets[1], context.pipelinelayout, ShaderLocation::computeset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth/2, context.fboheight/2, 1, computeconstants.BloomHBlurDispatch);

    bindresource(commandbuffer, context.bloompipeline[2], VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.scratchtargets[0], context.pipelinelayout, ShaderLocation::computeset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth/2, context.fboheight/2, 1, computeconstants.BloomVBlurDispatch);

    bindresource(commandbuffer, context.bloompipeline[3], VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.colorbuffertarget, context.pipelinelayout, ShaderLocation::computeset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth, context.fboheight, 1, computeconstants.BloomToneDispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 8);

  //
  // Sprite
  //

  beginpass(commandbuffer, context.renderpass, context.framebuffer, 0, 0, context.fbowidth, context.fboheight, extentof(clearvalues), clearvalues);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Sprites:
        draw_sprites(context, commandbuffer, *renderable_cast<Renderable::Sprites>(&renderable));
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

    blit(commandbuffer, context.colorbuffer.image, 0, 0, context.fbowidth, context.fboheight, viewport.image, viewport.x, viewport.y, viewport.width, viewport.height, VK_FILTER_LINEAR);

    transition_present(commandbuffer, viewport.image);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 10);

  barrier(commandbuffer);

  end(context.device, commandbuffer);

  //
  // Submit
  //

  BEGIN_TIMED_BLOCK(Wait, Color3(0.1f, 0.1f, 0.1f))

  wait(context.device, context.framefence);

  END_TIMED_BLOCK(Wait)

  // Feedback

  context.luminance = ((ComputeSet volatile *)(context.transfermemory.data()))->luminance;

  // Timing Queries

  uint64_t timings[16];
  retreive_querypool(context.device, context.timingquerypool, 0, 11, timings);

  GPU_TIMED_BLOCK(Shadows, Color3(0.0f, 0.4f, 0.0f), timings[0], timings[1])
  GPU_TIMED_BLOCK(Geometry, Color3(0.4f, 0.0f, 0.4f), timings[1], timings[2])
  GPU_TIMED_BLOCK(SSAO, Color3(0.2f, 0.8f, 0.2f), timings[2], timings[3])
  GPU_TIMED_BLOCK(Lighting, Color3(0.0f, 0.6f, 0.4f), timings[3], timings[4])
  GPU_TIMED_BLOCK(SkyBox, Color3(0.0f, 0.4f, 0.4f), timings[4], timings[5])
  GPU_TIMED_BLOCK(SSR, Color3(0.0f, 0.4f, 0.8f), timings[5], timings[6])
  GPU_TIMED_BLOCK(Luminance, Color3(0.8f, 0.4f, 0.2f), timings[6], timings[7])
  GPU_TIMED_BLOCK(Bloom, Color3(0.2f, 0.2f, 0.6f), timings[7], timings[8])
  GPU_TIMED_BLOCK(Sprites, Color3(0.4f, 0.4f, 0.0f), timings[8], timings[9])
  GPU_TIMED_BLOCK(Blit, Color3(0.4f, 0.4f, 0.4f), timings[9], timings[10])

  GPU_SUBMIT();

  submit(context.device, commandbuffer, viewport.acquirecomplete, viewport.rendercomplete, context.framefence);

  context.prevcamera = camera;

  ++context.frame;
}

