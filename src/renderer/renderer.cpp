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
#include <algorithm>
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
  vertex_position = 0,
  vertex_texcoord = 1,
  vertex_normal = 2,
  vertex_tangent = 3,

  sceneset = 0,
  materialset = 1,
  modelset = 2,
  computeset = 3,

  albedomap = 1,
  specularmap = 2,
  normalmap = 3,
  depthmap = 4,
  ssaomap = 5,
  shadowmap = 6,

  scratchmap0 = 1,
  scratchmap1 = 2,
  scratchmap2 = 3,
  scratchmap3 = 4,
};

constexpr size_t kPushConstantSize = 64;
constexpr size_t kTransferBufferSize = 64*1024;

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

struct SceneSet
{
  Matrix4f proj;
  Matrix4f invproj;
  Matrix4f view;
  Matrix4f invview;
  Matrix4f prevview;
  Matrix4f skyview;

  Vec3 camerapos;
  float exposure;

  Vec4 noise[16];
  Vec4 kernel[16];

  array<Matrix4f, ShadowMap::nslices> shadowview;

  MainLight mainlight;

  uint32_t pointlightcount;
  PointLight pointlights[256];
};


//|---------------------- PushBuffer ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// PushBuffer::Constructor ///////////////////////////
PushBuffer::PushBuffer(allocator_type const &allocator, size_t slabsize)
{
  m_slabsize = slabsize;
  m_slab = allocate<char, alignof(Header)>(allocator, m_slabsize);

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

namespace
{
  ///////////////////////// acquire_transferbuffer //////////////////////////
  size_t acquire_transferbuffer(RenderContext &context, size_t required)
  {
    auto bytes = alignto(required, context.device.physicaldeviceproperties.limits.minStorageBufferOffsetAlignment);

    size_t offset = context.offset.load(std::memory_order_relaxed);

    while (!context.offset.compare_exchange_weak(offset, offset + bytes))
      ;

    assert(offset + bytes < kTransferBufferSize);

    return offset;
  }


  ///////////////////////// prepare_render_pipeline /////////////////////////
  bool prepare_render_pipeline(RenderContext &context, int width, int height)
  {
    if (context.fbowidth != width || context.fboheight != height)
    {
      vkDeviceWaitIdle(context.device);
      vkResetCommandBuffer(context.commandbuffers[0], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
      vkResetCommandBuffer(context.commandbuffers[1], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

      if (width == 0 || height == 0)
        return false;

      //
      // Shadow Map
      //

      context.shadows.shadowmap = create_attachment(context.device, context.shadows.width, context.shadows.height, context.shadows.nslices, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

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

      context.colorbuffer = create_attachment(context.device, width, height, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

      setimagelayout(context.device, context.colorbuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

      context.colorbuffertarget = allocate_descriptorset(context.device, context.descriptorpool, context.computelayout, context.colorbuffer.imageview);

      //
      // Geometry Attachment
      //

      context.albedobuffer = create_attachment(context.device, width, height, 1, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
      context.specularbuffer = create_attachment(context.device, width, height, 1, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
      context.normalbuffer = create_attachment(context.device, width, height, 1, 1, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

      //
      // Depth Attachment
      //

      context.depthbuffer = create_attachment(context.device, width, height, 1, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

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
      geometrybuffer[0] = context.albedobuffer.imageview;
      geometrybuffer[1] = context.specularbuffer.imageview;
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
      context.fbocrop = (height - min((int)(width / context.camera.aspect()), height)) / 2;

      //
      // Scratch Buffers
      //

      for(size_t i = 0; i < extentof(context.scratchbuffers); ++i)
      {
        context.scratchbuffers[i] = create_attachment(context.device, width, height, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

        setimagelayout(context.device, context.scratchbuffers[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

        context.scratchtargets[i] = allocate_descriptorset(context.device, context.descriptorpool, context.computelayout, context.scratchbuffers[i].imageview);
      }

      //
      // SSAO
      //

      context.ssao[0] = create_attachment(context.device, width, height, 1, 1, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
      context.ssao[1] = create_attachment(context.device, width, height, 1, 1, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

      setimagelayout(context.device, context.ssao[0].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
      setimagelayout(context.device, context.ssao[1].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

      clear(context.device, context.ssao[0].image, Color4(1.0, 1.0, 1.0, 1.0));

      context.ssaotarget = allocate_descriptorset(context.device, context.descriptorpool, context.computelayout, context.ssao[1].imageview);

      //
      // Scene Set
      //

      bindtexture(context.device, context.sceneset, ShaderLocation::albedomap, context.albedobuffer);
      bindtexture(context.device, context.sceneset, ShaderLocation::specularmap, context.specularbuffer);
      bindtexture(context.device, context.sceneset, ShaderLocation::normalmap, context.normalbuffer);
      bindtexture(context.device, context.sceneset, ShaderLocation::depthmap, context.depthbuffer);
      bindtexture(context.device, context.sceneset, ShaderLocation::ssaomap, context.ssao[0]);
      bindtexture(context.device, context.sceneset, ShaderLocation::shadowmap, context.shadows.shadowmap);

      //
      // Bloom Commands
      //

      bindtexture(context.device, context.bloombuffer, ShaderLocation::scratchmap0, context.colorbuffer);
      bindtexture(context.device, context.bloombuffer, ShaderLocation::scratchmap1, context.scratchbuffers[0]);
      bindtexture(context.device, context.bloombuffer, ShaderLocation::scratchmap2, context.scratchbuffers[1]);

      begin(context.device, context.bloomblendcommands, context.framebuffer, context.renderpass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

      bindresource(context.bloomblendcommands, context.bloompipeline[3], 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);

      bindresource(context.bloomblendcommands, context.bloombuffer, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_GRAPHICS);

      bindresource(context.bloomblendcommands, context.unitquad);

      draw(context.bloomblendcommands, context.unitquad.vertexcount, 1, 0, 0);

      end(context.device, context.bloomblendcommands);

      return false;
    }

    return true;
  }

} // namespace


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

    assert(kPushConstantSize <= context.device.physicaldeviceproperties.limits.maxPushConstantsSize);

    // DescriptorPool

    VkDescriptorPoolSize typecounts[3] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    typecounts[0].descriptorCount = 6;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[1].descriptorCount = 24;
    typecounts[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    typecounts[2].descriptorCount = 5;

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

    // Transfer Buffer

    context.transferbuffer = create_transferbuffer(context.device, kTransferBufferSize);
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

    context.vertexattributes[0].location = ShaderLocation::vertex_position;
    context.vertexattributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    context.vertexattributes[0].offset = VertexLayout::position_offset;

    context.vertexattributes[1].location = ShaderLocation::vertex_texcoord;
    context.vertexattributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    context.vertexattributes[1].offset = VertexLayout::texcoord_offset;

    context.vertexattributes[2].location = ShaderLocation::vertex_normal;
    context.vertexattributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    context.vertexattributes[2].offset = VertexLayout::normal_offset;

    context.vertexattributes[3].location = ShaderLocation::vertex_tangent;
    context.vertexattributes[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    context.vertexattributes[3].offset = VertexLayout::tangent_offset;
  }

  if (context.scenesetlayout == 0)
  {
    // Scene Set

    VkDescriptorSetLayoutBinding bindings[7] = {};
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

    VkDescriptorSetLayoutBinding bindings[1] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;

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
    constants[0].size = kPushConstantSize;

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
    attachments[1].format = VK_FORMAT_B8G8R8A8_SRGB;
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
    attachments[0].format = VK_FORMAT_B8G8R8A8_SRGB;
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

  if (context.sceneset == 0)
  {
    context.scenesetoffsets[0] = acquire_transferbuffer(context, sizeof(SceneSet));
    context.scenesetoffsets[1] = acquire_transferbuffer(context, sizeof(SceneSet));
    context.sceneset = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.transferbuffer, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
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

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
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

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.lightingpipeline = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
  }

  if (context.skyboxpipeline == 0)
  {
    //
    // Skybox Pipeline
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

    for(size_t i = 0; i < 2; ++i)
    {
      context.skyboxbuffers[i] = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.transferbuffer, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
      context.skyboxcommands[i] = allocate_commandbuffer(context.device, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    }
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

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.bloompipeline[0] = create_pipeline(context.device, context.pipelinecache, pipelineinfo);

    context.bloombuffer = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.transferbuffer, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
  }

  if (context.bloompipeline[1] == 0)
  {
    //
    // Bloom H-Blur  Pipeline
    //

    auto cs = assets->find(CoreAsset::bloom_hblur_comp);

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

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.bloompipeline[2] = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
  }

  if (context.bloompipeline[3] == 0)
  {
    //
    // Bloom Blend Pipeline
    //

    auto vs = assets->find(CoreAsset::bloom_blend_vert);
    auto fs = assets->find(CoreAsset::bloom_blend_frag);

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
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
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

    context.bloompipeline[3] = create_pipeline(context.device, context.pipelinecache, pipelineinfo);

    context.bloomblendcommands = allocate_commandbuffer(context.device, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
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

  if (context.whitediffuse == 0)
  {
    auto image = assets->find(CoreAsset::white_diffuse);

    if (!image)
      return false;

    asset_guard lock(assets);

    auto bits = assets->request(platform, image);

    if (!bits)
      return false;

    context.whitediffuse = create_texture(context.device, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, (char*)bits + sizeof(PackImagePayload));
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

    context.nominalnormal = create_texture(context.device, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, (char*)bits + sizeof(PackImagePayload));
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

  context.frame = 0;

  context.fbowidth = 0;
  context.fboheight = 0;

  context.initialised = true;

  return true;
}


///////////////////////// prepare_shadowview ////////////////////////////////
void prepare_shadowview(ShadowMap &shadowmap, Camera const &camera, Vec3 const &lightdirection)
{
  const int nsplits = shadowmap.nslices;
  const float lambda = shadowmap.shadowsplitlambda;
  const float znear = 0.1;
  const float zfar = shadowmap.shadowsplitfar;
  const float extrusion = 1000.0;

  float splits[nsplits+1] = { znear };

  for(int i = 1; i <= nsplits; ++i)
  {
    float alpha = (float)i / nsplits;
    float logdist = znear * pow(zfar / znear, alpha);
    float uniformdist = znear + (zfar -znear) * alpha;

    splits[i] = lerp(uniformdist, logdist, lambda);
  }

  auto up = (abs(dot(lightdirection, Vec3(0, 1, 0))) < 0.95) ? Vec3(0, 1, 0) : Vec3(0, 1, 0);

  auto snapview = Transform::lookat(Vec3(0,0,0), -lightdirection, up);

  for(int i = 0; i < nsplits; ++i)
  {
    auto frustum = camera.frustum(splits[i], splits[i+1]);

    auto radius = 0.5f * norm(frustum.corners[0] - frustum.corners[6]);

    auto frustumcentre = frustum.centre();

    frustumcentre = inverse(snapview) * frustumcentre;
    frustumcentre.x -= fmod(frustumcentre.x, (radius + radius) / shadowmap.width);
    frustumcentre.y -= fmod(frustumcentre.y, (radius + radius) / shadowmap.height);
    frustumcentre = snapview * frustumcentre;

    auto lightpos = frustumcentre - extrusion * lightdirection;

    auto lightview = Transform::lookat(lightpos, lightpos + lightdirection, up);

    auto lightproj = OrthographicProjection(-radius, -radius, radius, radius, 0.1f, extrusion + radius);

    shadowmap.shadowview[i] = lightproj * inverse(lightview).matrix();
  }
}


///////////////////////// prepare_sceneset //////////////////////////////////
void prepare_sceneset(RenderContext &context, PushBuffer const &renderables, RenderParams const &params)
{
  auto scene = (SceneSet*)(context.transfermemory + context.scenesetoffsets[context.frame & 1]);

  scene->proj = context.proj;
  scene->invproj = inverse(scene->proj);
  scene->view = ScaleMatrix(Vector4(1.0f, context.fbowidth / context.camera.aspect() / context.fboheight, 1.0f, 1.0f)) * context.view;
  scene->invview = inverse(scene->view);
  scene->prevview = ScaleMatrix(Vector4(1.0f, context.fbowidth / context.camera.aspect() / context.fboheight, 1.0f, 1.0f)) * context.prevcamera.view();
  scene->skyview = (params.skyboxorientation * Transform::rotation(context.camera.rotation())).matrix() * scene->invproj;
  scene->camerapos = context.camera.position();
  scene->exposure = context.camera.exposure();
  scene->shadowview = context.shadows.shadowview;

  memcpy(scene->noise, context.ssaonoise, sizeof(scene->noise));
  memcpy(scene->kernel, context.ssaokernel, sizeof(scene->kernel));

  auto &mainlight = scene->mainlight;
  auto &pointlightcount = scene->pointlightcount;
  auto &pointlights = scene->pointlights;

  mainlight.direction.xyz = params.sundirection;
  mainlight.intensity.rgb = params.sunintensity;

  pointlightcount = 0;

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

    assert(size < kTransferBufferSize);

    memcpy(context.transfermemory ? (void*)context.transfermemory : (void*)map_memory<uint8_t>(context.device, context.transferbuffer, 0, size), bitmap, size);

    blit(commandbuffer, context.transferbuffer, 0, width, height, viewport.image, (viewport.width - width)/2, (viewport.height - height)/2, width, height);
  }

  transition_present(commandbuffer, viewport.image);

  end(context.device, commandbuffer);

  submit(context.device, commandbuffer, viewport.aquirecomplete, viewport.rendercomplete, context.framefence);

  ++context.frame;
}


///////////////////////// render ////////////////////////////////////////////
void render(RenderContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params)
{
  if (!prepare_render_pipeline(context, viewport.width, viewport.height))
  {
    render_fallback(context, viewport);
    return;
  }

  context.camera = camera;
  context.proj = camera.proj();
  context.view = camera.view();

  prepare_shadowview(context.shadows, camera, params.sundirection);

  prepare_sceneset(context, renderables, params);

  auto &commandbuffer = context.commandbuffers[context.frame & 1];

  begin(context.device, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  reset_querypool(commandbuffer, context.timingquerypool, 0, 1);

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

  bindresource(commandbuffer, context.sceneset, context.pipelinelayout, ShaderLocation::sceneset, context.scenesetoffsets[context.frame & 1], VK_PIPELINE_BIND_POINT_COMPUTE);

  if (params.ssao)
  {
    bindresource(commandbuffer, context.ssaopipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.ssaotarget, context.pipelinelayout, ShaderLocation::computeset, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, (context.ssao[1].width+31)/32, (context.ssao[1].height+31)/32, 1);

    setimagelayout(commandbuffer, context.ssao[1].image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    setimagelayout(commandbuffer, context.ssao[0].image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    blit(commandbuffer, context.ssao[1].image, 0, 0, context.ssao[1].width, context.ssao[1].height, context.ssao[0].image, 0, 0);

    setimagelayout(commandbuffer, context.ssao[1].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    setimagelayout(commandbuffer, context.ssao[0].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
  }

  querytimestamp(commandbuffer, context.timingquerypool, 3);

  bindresource(commandbuffer, context.lightingpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  bindresource(commandbuffer, context.colorbuffertarget, context.pipelinelayout, ShaderLocation::computeset, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, (context.fbowidth+15)/16, (context.fboheight+15)/16, 1);

  querytimestamp(commandbuffer, context.timingquerypool, 4);

  //
  // Skybox
  //

  beginpass(commandbuffer, context.renderpass, context.framebuffer, 0, 0, context.fbowidth, context.fboheight, extentof(clearvalues), clearvalues);

  draw_skybox(context, commandbuffer, params);

  endpass(commandbuffer, context.renderpass);

  querytimestamp(commandbuffer, context.timingquerypool, 5);

  //
  // Bloom
  //

  if (params.bloom)
  {
    bindresource(commandbuffer, context.bloompipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.scratchtargets[0], context.pipelinelayout, ShaderLocation::computeset, VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.bloombuffer, context.pipelinelayout, ShaderLocation::sceneset, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, (context.fbowidth+15)/16, (context.fboheight+15)/16, 1);

    bindresource(commandbuffer, context.bloompipeline[1], VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.scratchtargets[1], context.pipelinelayout, ShaderLocation::computeset, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, (context.fbowidth/2+63)/64, context.fboheight, 1);

    bindresource(commandbuffer, context.bloompipeline[2], VK_PIPELINE_BIND_POINT_COMPUTE);

    bindresource(commandbuffer, context.scratchtargets[0], context.pipelinelayout, ShaderLocation::computeset, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fbowidth/2, (context.fboheight/2+63)/64, 1);

    beginpass(commandbuffer, context.renderpass, context.framebuffer, 0, context.fbocrop, context.fbowidth, context.fboheight - context.fbocrop - context.fbocrop, extentof(clearvalues), clearvalues);

    execute(commandbuffer, context.bloomblendcommands);

    endpass(commandbuffer, context.renderpass);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 6);

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

  querytimestamp(commandbuffer, context.timingquerypool, 7);

  endpass(commandbuffer, context.renderpass);

  //
  // Blit
  //

  transition_acquire(commandbuffer, viewport.image);

  blit(commandbuffer, context.colorbuffer.image, 0, 0, context.fbowidth, context.fboheight, viewport.image, viewport.x, viewport.y, viewport.width, viewport.height, VK_FILTER_NEAREST);

  transition_present(commandbuffer, viewport.image);

  querytimestamp(commandbuffer, context.timingquerypool, 8);

  end(context.device, commandbuffer);

  //
  // Submit
  //

  BEGIN_TIMED_BLOCK(Wait, Color3(0.1, 0.1, 0.1))

  wait(context.device, context.framefence);

  END_TIMED_BLOCK(Wait)

  // Timing Queries

  uint64_t timings[16];
  retreive_querypool(context.device, context.timingquerypool, 0, 9, timings);

  GPU_TIMED_BLOCK(Shadows, Color3(0.0, 0.4, 0.0), timings[0], timings[1])
  GPU_TIMED_BLOCK(Geometry, Color3(0.4, 0.0, 0.4), timings[1], timings[2])
  GPU_TIMED_BLOCK(SSAO, Color3(0.2, 0.8, 0.2), timings[2], timings[3])
  GPU_TIMED_BLOCK(Lighting, Color3(0.0, 0.6, 0.4), timings[3], timings[4])
  GPU_TIMED_BLOCK(Skybox, Color3(0.0, 0.4, 0.4), timings[4], timings[5])
  GPU_TIMED_BLOCK(Bloom, Color3(0.2, 0.2, 0.8), timings[5], timings[6])
  GPU_TIMED_BLOCK(Sprites, Color3(0.4, 0.4, 0.0), timings[6], timings[7])
  GPU_TIMED_BLOCK(Blit, Color3(0.4, 0.4, 0.4), timings[7], timings[8])

  GPU_SUBMIT();

  submit(context.device, commandbuffer, viewport.aquirecomplete, viewport.rendercomplete, context.framefence);

  context.prevcamera = camera;

  ++context.frame;
}
