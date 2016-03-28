//
// Datum - renderer
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "renderer.h"
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
using namespace vulkan;
using namespace DatumPlatform;
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

};


//|---------------------- PushBuffer ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// PushBuffer::Constructor ///////////////////////////
PushBuffer::PushBuffer(allocator_type const &allocator, std::size_t slabsize)
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
  bool prepare_render_pipeline(RendererContext &context, int width, int height)
  {
    if (context.fbowidth != width || context.fboheight != height)
    {
//      int width = viewport.width;
//      int height = min((int)(viewport.width / camera.aspect()), viewport.height);

      //
      // Color Buffer
      //

      VkImageCreateInfo colorbufferinfo = {};
      colorbufferinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      colorbufferinfo.imageType = VK_IMAGE_TYPE_2D;
      colorbufferinfo.format = VK_FORMAT_B8G8R8A8_UNORM;
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
      // Render Pass
      //

      VkAttachmentDescription attachments[1] = {};
      attachments[0].format = colorbufferinfo.format;
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

      vkResetCommandBuffer(context.commandbuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    }

    return true;
  }

} // namespace


///////////////////////// prepare_render_context ////////////////////////////
bool prepare_render_context(DatumPlatform::PlatformInterface &platform, RendererContext &context, AssetManager *assets)
{
  if (context.initialised)
    return true;

  if (context.device == 0)
  {
    //
    // Vulkan Device
    //

    auto renderdevice = platform.render_device();

    initialise_vulkan_device(&context.device, renderdevice.physicaldevice, renderdevice.device);

    context.framefence = create_fence(context.device, VK_FENCE_CREATE_SIGNALED_BIT);

    context.commandpool = create_commandpool(context.device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    context.commandbuffer = allocate_commandbuffer(context.device, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  }

  context.frame = 0;

  context.fbowidth = 0;
  context.fboheight = 0;

  context.initialised = true;

  return true;
}


///////////////////////// render_fallback ///////////////////////////////////
void render_fallback(RendererContext &context, DatumPlatform::Viewport const &viewport, void *bitmap, int width, int height)
{
  Buffer src;

  if (bitmap)
  {
    VkBufferCreateInfo bufferinfo = {};
    bufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferinfo.size = width * height * sizeof(uint32_t);

    src = create_buffer(context.device, bufferinfo);

    memcpy(map_memory(context.device, src, 0, bufferinfo.size), bitmap, bufferinfo.size);
  }

  wait(context.device, context.framefence);

  begin(context.device, context.commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  transition_aquire(context.device, context.commandbuffer, viewport.image);

  clear(context.device, context.commandbuffer, viewport.image, Color4(0.0f, 0.0f, 0.0f, 1.0f));

  if (bitmap)
  {
    blit(context.device, context.commandbuffer, src, width, height, viewport.image, (viewport.width - width)/2, (viewport.height - height)/2, width, height);
  }

  transition_present(context.device, context.commandbuffer, viewport.image);

  end(context.device, context.commandbuffer);

  submit(context.device, context.commandbuffer, 0, viewport.aquirecomplete, viewport.rendercomplete, context.framefence);

  vkWaitForFences(context.device, 1, context.framefence.data(), VK_TRUE, UINT64_MAX);

  ++context.frame;
}


///////////////////////// render ////////////////////////////////////////////
void render(RendererContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params)
{
  prepare_render_pipeline(context, viewport.width, viewport.height);

  wait(context.device, context.framefence);

  begin(context.device, context.commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

///
  VkClearValue clearvalues[1];
  clearvalues[0].color = { 1.0f, 0.0f, 0.0f, 1.0f };

  VkRenderPassBeginInfo renderpassbegininfo = {};
  renderpassbegininfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderpassbegininfo.renderPass = context.renderpass;
  renderpassbegininfo.renderArea.offset.x = 0;
  renderpassbegininfo.renderArea.offset.y = 0;
  renderpassbegininfo.renderArea.extent.width = context.fbowidth;
  renderpassbegininfo.renderArea.extent.height = context.fboheight;
  renderpassbegininfo.clearValueCount = 1;
  renderpassbegininfo.pClearValues = clearvalues;
  renderpassbegininfo.framebuffer = context.framebuffer;

  vkCmdBeginRenderPass(context.commandbuffer, &renderpassbegininfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdEndRenderPass(context.commandbuffer);
///

  transition_aquire(context.device, context.commandbuffer, viewport.image);

//  clear(context.device, context.commandbuffer, viewport.image, Color4(0.0f, 0.0f, 0.0f, 1.0f));

  blit(context.device, context.commandbuffer, context.colorbuffer, 0, 0, context.fbowidth, context.fboheight, viewport.image, viewport.x, viewport.y);
//  blit(context.device, context.commandbuffer, context.colorbuffer, 0, 0, context.fbowidth, context.fboheight, viewport.image, viewport.x, viewport.y, viewport.width, viewport.height, VK_FILTER_LINEAR);

  transition_present(context.device, context.commandbuffer, viewport.image);

  end(context.device, context.commandbuffer);

  submit(context.device, context.commandbuffer, 0, viewport.aquirecomplete, viewport.rendercomplete, context.framefence);

  ++context.frame;
}
