//
// Datum - renderer
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "renderer.h"
#include "commandlist.h"
#include "corepack.h"
#include "assetpack.h"
#include "leap/util.h"
#include "leap/lml/matrixconstants.h"
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

  albedomap = 1,
};

struct SceneSet
{
  Matrix4f proj;
  Matrix4f invproj;
  Matrix4f view;
  Matrix4f invview;

  Matrix4f worldview;
};

constexpr int kPushConstantSize = 64;
constexpr int kTransferBufferSize = 64*1024;


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

  ///////////////////////// prepare_render_pipeline /////////////////////////
  bool prepare_framebuffer(RenderContext &context, int width, int height)
  {
    if (context.fbowidth != width || context.fboheight != height)
    {
      if (width == 0 || height == 0)
        return false;

      //
      // Color Buffer
      //

      VkImageCreateInfo colorbufferinfo = {};
      colorbufferinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      colorbufferinfo.imageType = VK_IMAGE_TYPE_2D;
      colorbufferinfo.format = VK_FORMAT_B8G8R8A8_SRGB;
      colorbufferinfo.extent.width = width;
      colorbufferinfo.extent.height = height;
      colorbufferinfo.extent.depth = 1;
      colorbufferinfo.mipLevels = 1;
      colorbufferinfo.arrayLayers = 1;
      colorbufferinfo.samples = VK_SAMPLE_COUNT_1_BIT;
      colorbufferinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      colorbufferinfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      colorbufferinfo.flags = 0;

      context.colorbuffer = create_image(context.device, colorbufferinfo);

      setimagelayout(context.device, context.colorbuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

      //
      // Frame Buffer
      //

      VkImageViewCreateInfo colorbufferviewinfo = {};
      colorbufferviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      colorbufferviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      colorbufferviewinfo.format = colorbufferinfo.format;
      colorbufferviewinfo.flags = 0;
      colorbufferviewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
      colorbufferviewinfo.image = context.colorbuffer;

      context.colorbufferview = create_imageview(context.device, colorbufferviewinfo);

      VkFramebufferCreateInfo framebufferinfo = {};
      framebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferinfo.renderPass = context.renderpass;
      framebufferinfo.attachmentCount = 1;
      framebufferinfo.pAttachments = context.colorbufferview.data();
      framebufferinfo.width = width;
      framebufferinfo.height = height;
      framebufferinfo.layers = 1;

      context.framebuffer = create_framebuffer(context.device, framebufferinfo);

      context.fbowidth = width;
      context.fboheight = height;

      vkResetCommandBuffer(context.commandbuffers[0], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
      vkResetCommandBuffer(context.commandbuffers[1], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    }

    return true;
  }

  ///////////////////////// draw_sprites ////////////////////////////////////
  void draw_sprites(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Sprites const &sprites)
  {
    execute(commandbuffer, *sprites.spritelist);
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

    VkDescriptorPoolSize typecounts[2] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    typecounts[0].descriptorCount = 1;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    typecounts[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo descriptorpoolinfo = {};
    descriptorpoolinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorpoolinfo.maxSets = accumulate(begin(typecounts), end(typecounts), 0, [](int i, auto &k) { return i += k.descriptorCount; });
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

    VkDescriptorSetLayoutBinding bindings[1] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.scenesetlayout = create_descriptorsetlayout(context.device, createinfo);

    context.sceneset = allocate_descriptorset(context.device, context.descriptorpool, context.scenesetlayout, context.transferbuffer, 0, sizeof(SceneSet), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  }

  if (context.materialsetlayout == 0)
  {
    // Material Set

    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;

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

  if (context.pipelinelayout == 0)
  {
    // PipelineLayout

    VkPushConstantRange constants[1] = {};
    constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    constants[0].offset = 0;
    constants[0].size = kPushConstantSize;

    VkDescriptorSetLayout layouts[3] = {};
    layouts[0] = context.scenesetlayout;
    layouts[1] = context.materialsetlayout;
    layouts[2] = context.modelsetlayout;

    VkPipelineLayoutCreateInfo pipelinelayoutinfo = {};
    pipelinelayoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelinelayoutinfo.pushConstantRangeCount = extentof(constants);
    pipelinelayoutinfo.pPushConstantRanges = constants;
    pipelinelayoutinfo.setLayoutCount = extentof(layouts);
    pipelinelayoutinfo.pSetLayouts = layouts;

    context.pipelinelayout = create_pipelinelayout(context.device, pipelinelayoutinfo);
  }

  if (context.renderpass == 0)
  {
    //
    // Render Pass
    //

    VkAttachmentDescription attachments[1] = {};
    attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    VkAttachmentReference colorbufferreference = {};
    colorbufferreference.attachment = 0;
    colorbufferreference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorbufferreference;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = 1;
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = 1;
    renderpassinfo.pSubpasses = &subpass;
    renderpassinfo.dependencyCount = 0;
    renderpassinfo.pDependencies = nullptr;

    context.renderpass = create_renderpass(context.device, renderpassinfo);
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

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = 1;
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.spritepipeline = create_pipeline(context.device, context.pipelinecache, pipelineinfo);
  }

  context.frame = 0;

  context.fbowidth = 0;
  context.fboheight = 0;

  context.initialised = true;

  return true;
}


///////////////////////// render_fallback ///////////////////////////////////
void render_fallback(RenderContext &context, DatumPlatform::Viewport const &viewport, void *bitmap, int width, int height)
{
  assert(!context.initialised);

  CommandBuffer &commandbuffer = context.commandbuffers[context.frame & 1];

  wait(context.device, context.framefence);

  begin(context.device, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  transition_acquire(commandbuffer, viewport.image);

  clear(commandbuffer, viewport.image, Color4(0.0f, 0.0f, 0.0f, 1.0f));

  if (bitmap)
  {
    size_t size = width * height * sizeof(uint32_t);

    assert(size < kTransferBufferSize);

    memcpy(map_memory<void*>(context.device, context.transferbuffer, 0, size), bitmap, size);

    blit(commandbuffer, context.transferbuffer, 0, width, height, viewport.image, (viewport.width - width)/2, (viewport.height - height)/2, width, height);
  }

  transition_present(commandbuffer, viewport.image);

  end(context.device, commandbuffer);

  submit(context.device, commandbuffer, viewport.aquirecomplete, viewport.rendercomplete, context.framefence);
}


///////////////////////// render ////////////////////////////////////////////
void render(RenderContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params)
{
  prepare_framebuffer(context, viewport.width, viewport.height);

  CommandBuffer &commandbuffer = context.commandbuffers[context.frame & 1];

  SceneSet scene;
  scene.proj = OrthographicProjection((float)viewport.x, (float)viewport.y, (float)(viewport.x + viewport.width), (float)(viewport.y + viewport.height), 0.0f, 1000.0f);
  scene.invproj = inverse(scene.proj);
  scene.view = IdentityMatrix<float, 4>();
  scene.invview = inverse(scene.view);
  scene.worldview = scene.proj * scene.view;

  begin(context.device, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  reset_querypool(commandbuffer, context.timingquerypool, 0, 1);

  querytimestamp(commandbuffer, context.timingquerypool, 0);

  update(commandbuffer, context.transferbuffer, 0, sizeof(scene), &scene);

  //
  // Sprite
  //

  beginpass(commandbuffer, context.renderpass, context.framebuffer, 0, 0, context.fbowidth, context.fboheight, Color4(0.0f, 0.0f, 0.0f, 1.0f));

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

  querytimestamp(commandbuffer, context.timingquerypool, 1);

  //
  // Blit
  //

  transition_acquire(commandbuffer, viewport.image);

  blit(commandbuffer, context.colorbuffer, 0, 0, context.fbowidth, context.fboheight, viewport.image, viewport.x, viewport.y);

  transition_present(commandbuffer, viewport.image);

  querytimestamp(commandbuffer, context.timingquerypool, 2);

  end(context.device, commandbuffer);

  //
  // Submit
  //

  BEGIN_TIMED_BLOCK(Wait, Color3(0.1, 0.1, 0.1))

  wait(context.device, context.framefence);

  END_TIMED_BLOCK(Wait)

  // Timing Queries

  uint64_t timings[16];
  retreive_querypool(context.device, context.timingquerypool, 0, 3, timings);

  GPU_TIMED_BLOCK(Sprites, Color3(0.4, 0.4, 0.0), timings[0], timings[1])
  GPU_TIMED_BLOCK(Blit, Color3(0.4, 0.4, 0.4), timings[1], timings[2])

  GPU_SUBMIT();

  submit(context.device, commandbuffer, viewport.aquirecomplete, viewport.rendercomplete, context.framefence);

  ++context.frame;
}
