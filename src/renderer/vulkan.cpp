//
// Datum - vulkan interface
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "vulkan.h"
#include "debug.h"

using namespace std;
using namespace lml;

namespace Vulkan
{

  //|---------------------- VulkanDevice ------------------------------------
  //|------------------------------------------------------------------------

  ///////////////////////// initialise_vulkan_device ////////////////////////
  void initialise_vulkan_device(VulkanDevice *vulkan, VkPhysicalDevice physicaldevice, VkDevice device)
  {
    vulkan->physicaldevice = physicaldevice;

    vkGetPhysicalDeviceProperties(vulkan->physicaldevice, &vulkan->physicaldeviceproperties);

    vkGetPhysicalDeviceMemoryProperties(vulkan->physicaldevice, &vulkan->physicaldevicememoryproperties);

    vulkan->device = device;

    uint32_t queuecount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan->physicaldevice, &queuecount, nullptr);

    assert(queuecount < 16);

    VkQueueFamilyProperties queueproperties[16];
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan->physicaldevice, &queuecount, queueproperties);

    uint32_t queueindex = 0;
    while (queueindex < queuecount && !(queueproperties[queueindex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
      ++queueindex;

    vkGetDeviceQueue(vulkan->device, queueindex, 0, &vulkan->queue);   
  }


  ///////////////////////// allocate_memory /////////////////////////////////
  VkResult allocate_memory(VulkanDevice const &vulkan, VkMemoryRequirements const &requirements, VkMemoryPropertyFlags properties, VkDeviceMemory *memory)
  {
    VkMemoryAllocateInfo allocateinfo = {};
    allocateinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateinfo.memoryTypeIndex = 0;
    allocateinfo.allocationSize = requirements.size;

    for (uint32_t i = 0; i < 32; i++)
    {
      if ((requirements.memoryTypeBits >> i) & 1)
      {
        if ((vulkan.physicaldevicememoryproperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
          allocateinfo.memoryTypeIndex = i;
          break;
        }
      }
    }

    return vkAllocateMemory(vulkan.device, &allocateinfo, nullptr, memory);
  }


  ///////////////////////// create_commandpool //////////////////////////////
  CommandPool create_commandpool(VulkanDevice const &vulkan, VkCommandPoolCreateFlags flags)
  {
    uint32_t queuecount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan.physicaldevice, &queuecount, nullptr);

    VkQueueFamilyProperties queueproperties[16];
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan.physicaldevice, &queuecount, queueproperties);

    uint32_t queueindex = 0;
    while (queueindex < queuecount && !(queueproperties[queueindex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
      ++queueindex;

    VkCommandPoolCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createinfo.flags = flags;
    createinfo.queueFamilyIndex = queueindex;

    VkCommandPool commandpool;
    if (vkCreateCommandPool(vulkan.device, &createinfo, nullptr, &commandpool) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateCommandPool failed");

    return { commandpool, { vulkan.device } };
  }


  ///////////////////////// create_descriptorsetlayout //////////////////////
  DescriptorSetLayout create_descriptorsetlayout(VulkanDevice const &vulkan, VkDescriptorSetLayoutCreateInfo const &createinfo)
  {
    VkDescriptorSetLayout descriptorsetlayout;

    if (vkCreateDescriptorSetLayout(vulkan.device, &createinfo, nullptr, &descriptorsetlayout) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateDescriptorSetLayout failed");

    return { descriptorsetlayout, { vulkan.device } };
  }


  ///////////////////////// create_pipelinelayout ///////////////////////////
  PipelineLayout create_pipelinelayout(VulkanDevice const &vulkan, VkPipelineLayoutCreateInfo const &createinfo)
  {
    VkPipelineLayout pipelinelayout;

    if (vkCreatePipelineLayout(vulkan.device, &createinfo, nullptr, &pipelinelayout) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreatePipelineLayout failed");

    return { pipelinelayout, { vulkan.device } };
  }


  ///////////////////////// create_pipeline /////////////////////////////////
  Pipeline create_pipeline(VulkanDevice const &vulkan, VkPipelineCache cache, VkGraphicsPipelineCreateInfo const &createinfo)
  {
    VkPipeline pipeline;

    if (vkCreateGraphicsPipelines(vulkan.device, cache, 1, &createinfo, nullptr, &pipeline) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateGraphicsPipelines failed");

    return { pipeline, { vulkan.device } };
  }


  ///////////////////////// create_pipelinecache ////////////////////////////
  PipelineCache create_pipelinecache(VulkanDevice const &vulkan, VkPipelineCacheCreateInfo const &createinfo)
  {
    VkPipelineCache pipelinecache;

    if (vkCreatePipelineCache(vulkan.device, &createinfo, nullptr, &pipelinecache) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreatePipelineCache failed");

    return { pipelinecache, { vulkan.device } };
  }


  ///////////////////////// create_descriptorpool ///////////////////////////
  DescriptorPool create_descriptorpool(VulkanDevice const &vulkan, VkDescriptorPoolCreateInfo const &createinfo)
  {
    VkDescriptorPool descriptorpool;

    if (vkCreateDescriptorPool(vulkan.device, &createinfo, nullptr, &descriptorpool) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateDescriptorPool failed");

    return { descriptorpool, { vulkan.device } };
  }


  ///////////////////////// create_shadermodule /////////////////////////////
  ShaderModule create_shadermodule(VulkanDevice const &vulkan, const void *code, size_t size)
  {
    VkShaderModuleCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createinfo.codeSize = size;
    createinfo.pCode = (uint32_t const *)code;
    createinfo.flags = 0;

    VkShaderModule shadermodule;

    if (vkCreateShaderModule(vulkan.device, &createinfo, nullptr, &shadermodule) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateShaderModule failed");

    return { shadermodule, { vulkan.device } };
  }


  ///////////////////////// create_renderpass ///////////////////////////////
  RenderPass create_renderpass(VulkanDevice const &vulkan, VkRenderPassCreateInfo const &createinfo)
  {
    VkRenderPass renderpass;

    if (vkCreateRenderPass(vulkan.device, &createinfo, nullptr, &renderpass) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateRenderPass failed");

    return { renderpass, { vulkan.device } };
  }


  ///////////////////////// create_framebuffer ///////////////////////////////
  FrameBuffer create_framebuffer(VulkanDevice const &vulkan, VkFramebufferCreateInfo const &createinfo)
  {
    VkFramebuffer framebuffer;

    if (vkCreateFramebuffer(vulkan.device, &createinfo, nullptr, &framebuffer) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateFrameBuffer failed");

    return { framebuffer, { vulkan.device } };
  }


  ///////////////////////// create_image ////////////////////////////////////
  Image create_image(VulkanDevice const &vulkan, VkImageCreateInfo const &createinfo)
  {
    VkImage image;

    if (vkCreateImage(vulkan.device, &createinfo, nullptr, &image) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateImage failed");

    VkMemoryRequirements memoryrequirements;
    vkGetImageMemoryRequirements(vulkan.device, image, &memoryrequirements);

    VkDeviceMemory memory;
    if (allocate_memory(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory) != VK_SUCCESS)
      throw runtime_error("Vulkan vkAllocateMemory failed");

    if (vkBindImageMemory(vulkan.device, image, memory, 0) != VK_SUCCESS)
      throw runtime_error("Vulkan vkBindImageMemory failed");

    return { image, { vulkan.device, memory } };
  }


  ///////////////////////// create_imageview ////////////////////////////////
  ImageView create_imageview(VulkanDevice const &vulkan, VkImageViewCreateInfo const &createinfo)
  {
    VkImageView imageview;

    if (vkCreateImageView(vulkan.device, &createinfo, nullptr, &imageview) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateImageView failed");

    return { imageview, { vulkan.device } };
  }


  ///////////////////////// create_transferbuffer ///////////////////////////
  TransferBuffer create_transferbuffer(VulkanDevice const &vulkan, VkDeviceSize size)
  {
    VkBufferCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createinfo.size = size;

    VkBuffer transferbuffer;
    vkCreateBuffer(vulkan.device, &createinfo, nullptr, &transferbuffer);

    VkMemoryRequirements memoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, transferbuffer, &memoryrequirements);

    VkDeviceMemory memory;
    allocate_memory(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory);

    vkBindBufferMemory(vulkan.device, transferbuffer, memory, 0);

    return { size, { transferbuffer, { vulkan.device } }, { memory, { vulkan.device } } };
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, const void *vertices, size_t vertexcount, size_t vertexsize)
  {
    VkBufferCreateInfo vertexbufferinfo = {};
    vertexbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexbufferinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertexbufferinfo.size = vertexcount * vertexsize;

    VkBuffer vertexbuffer;
    vkCreateBuffer(vulkan.device, &vertexbufferinfo, nullptr, &vertexbuffer);

    VkMemoryRequirements memoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, vertexbuffer, &memoryrequirements);

    VkDeviceMemory memory;
    allocate_memory(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory);

    vkBindBufferMemory(vulkan.device, vertexbuffer, memory, 0);

    memcpy(map_memory(vulkan, memory, 0, vertexbufferinfo.size), vertices, vertexbufferinfo.size);

    return { vertexcount, { vertexbuffer, { vulkan.device } }, 0, { }, { memory, { vulkan.device } } };
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, const void *vertices, size_t vertexcount, size_t vertexsize, const void *indices, size_t indexcount, size_t indexsize)
  {
    VkBufferCreateInfo vertexbufferinfo = {};
    vertexbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexbufferinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertexbufferinfo.size = vertexcount * vertexsize;

    VkBuffer vertexbuffer;
    vkCreateBuffer(vulkan.device, &vertexbufferinfo, nullptr, &vertexbuffer);

    VkMemoryRequirements vertexmemoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, vertexbuffer, &vertexmemoryrequirements);

    VkBufferCreateInfo indexbufferinfo = {};
    indexbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexbufferinfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    indexbufferinfo.size = indexcount * indexsize;

    VkBuffer indexbuffer;
    vkCreateBuffer(vulkan.device, &indexbufferinfo, nullptr, &indexbuffer);

    VkMemoryRequirements indexmemoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, indexbuffer, &indexmemoryrequirements);

    VkDeviceSize padding = indexmemoryrequirements.alignment - (vertexmemoryrequirements.size % indexmemoryrequirements.alignment);

    VkMemoryRequirements memoryrequirements;
    memoryrequirements.alignment = vertexmemoryrequirements.alignment;
    memoryrequirements.memoryTypeBits = vertexmemoryrequirements.memoryTypeBits & indexmemoryrequirements.memoryTypeBits;
    memoryrequirements.size = vertexmemoryrequirements.size + padding + indexmemoryrequirements.size;

    VkDeviceMemory memory;
    allocate_memory(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory);

    vkBindBufferMemory(vulkan.device, vertexbuffer, memory, 0);
    vkBindBufferMemory(vulkan.device, indexbuffer, memory, vertexmemoryrequirements.size + padding);

    memcpy(map_memory(vulkan, memory, 0, vertexbufferinfo.size), vertices, vertexbufferinfo.size);
    memcpy(map_memory(vulkan, memory, vertexmemoryrequirements.size + padding, indexbufferinfo.size), indices, indexbufferinfo.size);

    return { vertexcount, { vertexbuffer, { vulkan.device } }, indexcount, { indexbuffer, { vulkan.device } }, { memory, { vulkan.device } } };
  }


  ///////////////////////// map_memory //////////////////////////////////////
  Memory map_memory(VulkanDevice const &vulkan, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size)
  {
    void *data = nullptr;

    vkMapMemory(vulkan.device, memory, offset, size, 0, &data);

    return { data, { vulkan.device, memory } };
  }


  ///////////////////////// create_fence ////////////////////////////////////
  Fence create_fence(VulkanDevice const &vulkan, VkFenceCreateFlags flags)
  {
    VkFenceCreateInfo fenceinfo = {};
    fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceinfo.flags = flags;

    VkFence fence;
    if (vkCreateFence(vulkan.device, &fenceinfo, nullptr, &fence) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateFence failed");

    return { fence, { vulkan.device } };
  }


  ///////////////////////// wait ////////////////////////////////////////////
  void wait(VulkanDevice const &vulkan, VkFence fence)
  {
    vkWaitForFences(vulkan.device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkResetFences(vulkan.device, 1, &fence);
  }


  ///////////////////////// signal //////////////////////////////////////////
  void signal(VulkanDevice const &vulkan, VkFence fence)
  {
    vkQueueSubmit(vulkan.queue, 0, nullptr, fence);
  }


  ///////////////////////// allocate_descriptorset //////////////////////////
  DescriptorSet allocate_descriptorset(VulkanDevice const &vulkan, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkDescriptorType type)
  {
    VkDescriptorSetAllocateInfo allocateinfo = {};
    allocateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateinfo.descriptorPool = pool;
    allocateinfo.descriptorSetCount = 1;
    allocateinfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorset;
    if (vkAllocateDescriptorSets(vulkan.device, &allocateinfo, &descriptorset) != VK_SUCCESS)
      throw runtime_error("Vulkan vkAllocateDescriptorSets failed");

    VkDescriptorBufferInfo bufferinfo = {};
    bufferinfo.buffer = buffer;
    bufferinfo.offset = offset;
    bufferinfo.range = size;

    VkWriteDescriptorSet writeset = {};
    writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeset.dstSet = descriptorset;
    writeset.dstBinding = 0;
    writeset.descriptorCount = 1;
    writeset.descriptorType = type;
    writeset.pBufferInfo = &bufferinfo;

    vkUpdateDescriptorSets(vulkan.device, 1, &writeset, 0, nullptr);

    return { descriptorset, { vulkan.device, pool } };
  }


  ///////////////////////// reset_descriptorpool ////////////////////////////
  void reset_descriptorpool(VulkanDevice const &vulkan, VkDescriptorPool descriptorpool)
  {
    vkResetDescriptorPool(vulkan.device, descriptorpool, 0);
  }


  ///////////////////////// allocate_commandbuffer //////////////////////////
  CommandBuffer allocate_commandbuffer(VulkanDevice const &vulkan, VkCommandPool pool, VkCommandBufferLevel level)
  {
    VkCommandBufferAllocateInfo allocateinfo = {};
    allocateinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateinfo.commandPool = pool;
    allocateinfo.level = level;
    allocateinfo.commandBufferCount = 1;

    VkCommandBuffer commandbuffer;
    if (vkAllocateCommandBuffers(vulkan.device, &allocateinfo, &commandbuffer) != VK_SUCCESS)
      throw runtime_error("Vulkan vkAllocateCommandBuffers failed");

    return { commandbuffer, { vulkan.device, pool } };
  }


  ///////////////////////// reset_commandpool ///////////////////////////////
  void reset_commandpool(VulkanDevice const &vulkan, VkCommandPool commandpool)
  {
    vkResetCommandPool(vulkan.device, commandpool, 0);
  }


  ///////////////////////// begin ///////////////////////////////////////////
  void begin(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkCommandBufferUsageFlags flags)
  {
    VkCommandBufferBeginInfo begininfo = {};
    begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begininfo.flags = flags;

    vkBeginCommandBuffer(commandbuffer, &begininfo);
  }


  ///////////////////////// begin ///////////////////////////////////////////
  void begin(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkCommandBufferInheritanceInfo const &inheritanceinfo, VkCommandBufferUsageFlags flags)
  {
    VkCommandBufferBeginInfo begininfo = {};
    begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begininfo.pInheritanceInfo = &inheritanceinfo;
    begininfo.flags = flags;

    vkBeginCommandBuffer(commandbuffer, &begininfo);
  }


  ///////////////////////// end /////////////////////////////////////////////
  void end(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer)
  {
    vkEndCommandBuffer(commandbuffer);
  }


  ///////////////////////// submit //////////////////////////////////////////
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkPipelineStageFlags flags)
  {
    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.pWaitDstStageMask = &flags;
    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &commandbuffer;

    vkQueueSubmit(vulkan.queue, 1, &submitinfo, VK_NULL_HANDLE);
  }

  ///////////////////////// submit //////////////////////////////////////////
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkPipelineStageFlags flags, VkSemaphore waitsemaphore, VkSemaphore signalsemaphore, VkFence fence)
  {
    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.pWaitDstStageMask = &flags;
    submitinfo.waitSemaphoreCount = 1;
    submitinfo.pWaitSemaphores = &waitsemaphore;
    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &commandbuffer;
    submitinfo.signalSemaphoreCount = 1;
    submitinfo.pSignalSemaphores = &signalsemaphore;

    vkQueueSubmit(vulkan.queue, 1, &submitinfo, fence);
  }


  ///////////////////////// transition_acquire //////////////////////////////
  void transition_acquire(VkCommandBuffer commandbuffer, VkImage image)
  {
    VkImageMemoryBarrier imagebarrier = {};
    imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imagebarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imagebarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imagebarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imagebarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imagebarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imagebarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imagebarrier.image = image;
    imagebarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);
  }


  ///////////////////////// transition_present //////////////////////////////
  void transition_present(VkCommandBuffer commandbuffer, VkImage image)
  {
    VkImageMemoryBarrier imagebarrier = {};
    imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imagebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imagebarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imagebarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imagebarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imagebarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imagebarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imagebarrier.image = image;
    imagebarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);
  }


  ///////////////////////// clear ///////////////////////////////////////////
  void clear(VkCommandBuffer commandbuffer, VkImage image, Color4 const &color)
  {
    VkClearColorValue clearcolor;
    clearcolor.float32[0] = color.r;
    clearcolor.float32[1] = color.g;
    clearcolor.float32[2] = color.b;
    clearcolor.float32[3] = color.a;

    VkImageSubresourceRange subresourcerange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdClearColorImage(commandbuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearcolor, 1, &subresourcerange);
  }


  ///////////////////////// update //////////////////////////////////////////
  void update(VkCommandBuffer commandbuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, void const *data)
  {
    vkCmdUpdateBuffer(commandbuffer, buffer, offset, size, (uint32_t const *)data);
  }


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkImage src, int sx, int sy, int sw, int sh, VkImage dst, int dx, int dy)
  {
    VkImageCopy imagecopy = {};

    imagecopy.srcOffset.x = sx;
    imagecopy.srcOffset.y = sy;
    imagecopy.srcOffset.z = 0;
    imagecopy.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0,  0, 1 };

    imagecopy.dstOffset.x = dx;
    imagecopy.dstOffset.y = dy;
    imagecopy.dstOffset.z = 1;
    imagecopy.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

    imagecopy.extent.width = sw;
    imagecopy.extent.height = sh;
    imagecopy.extent.depth = 1;

    vkCmdCopyImage(commandbuffer, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imagecopy);
  }


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkImage src, int sx, int sy, int sw, int sh, VkImage dst, int dx, int dy, int dw, int dh, VkFilter filter)
  {
    VkImageBlit imageblit = {};

    imageblit.srcOffsets[0].x = sx;
    imageblit.srcOffsets[0].y = sy;
    imageblit.srcOffsets[0].z = 0;
    imageblit.srcOffsets[1].x = sw + sx;
    imageblit.srcOffsets[1].y = sh + sy;
    imageblit.srcOffsets[1].z = 1;
    imageblit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0,  0, 1 };

    imageblit.dstOffsets[0].x = dx;
    imageblit.dstOffsets[0].y = dy;
    imageblit.dstOffsets[0].z = 0;
    imageblit.dstOffsets[1].x = dw + dx;
    imageblit.dstOffsets[1].y = dh + dy;
    imageblit.dstOffsets[1].z = 1;
    imageblit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

    vkCmdBlitImage(commandbuffer, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageblit, filter);
  }


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize offset, int sw, int sh, VkImage dst, int dx, int dy, int dw, int dh)
  {
    VkBufferImageCopy buffercopy = {};

    buffercopy.bufferOffset = offset;
    buffercopy.bufferRowLength = sw;
    buffercopy.bufferImageHeight = sh;

    buffercopy.imageOffset.x = dx;
    buffercopy.imageOffset.y = dy;
    buffercopy.imageOffset.z = 1;
    buffercopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

    buffercopy.imageExtent.width = dw;
    buffercopy.imageExtent.height = dh;
    buffercopy.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(commandbuffer, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffercopy);
  }


  ///////////////////////// setimagelayout //////////////////////////////////
  void setimagelayout(VkCommandBuffer commandbuffer, VkImage image, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange)
  {
    VkImageMemoryBarrier imagebarrier = {};
    imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imagebarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imagebarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imagebarrier.oldLayout = oldlayout;
    imagebarrier.newLayout = newlayout;
    imagebarrier.image = image;
    imagebarrier.subresourceRange = subresourcerange;

    if (oldlayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      imagebarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (newlayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
      imagebarrier.dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;

    if (newlayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      imagebarrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (newlayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      imagebarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    if (newlayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
      imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

    vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);
  }

  ///////////////////////// setimagelayout //////////////////////////////////
  void setimagelayout(VulkanDevice const &vulkan, VkImage image, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange)
  {
    CommandPool setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    CommandBuffer setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    setimagelayout(setupbuffer, image, oldlayout, newlayout, subresourcerange);

    end(vulkan, setupbuffer);

    submit(vulkan, setupbuffer);

    vkQueueWaitIdle(vulkan.queue);
  }


  ///////////////////////// beginpass ///////////////////////////////////////
  void beginpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass, VkFramebuffer framebuffer, int x, int y, int width, int height, Color4 const &clearcolor)
  {
    VkClearValue clearvalues[1];
    clearvalues[0].color = { clearcolor.r, clearcolor.g, clearcolor.b, clearcolor.a };

    VkRenderPassBeginInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassinfo.renderPass = renderpass;
    renderpassinfo.framebuffer = framebuffer;
    renderpassinfo.renderArea.offset.x = x;
    renderpassinfo.renderArea.offset.y = y;
    renderpassinfo.renderArea.extent.width = width;
    renderpassinfo.renderArea.extent.height = height;
    renderpassinfo.clearValueCount = 1;
    renderpassinfo.pClearValues = clearvalues;

    vkCmdBeginRenderPass(commandbuffer, &renderpassinfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
  }


  ///////////////////////// endpass /////////////////////////////////////////
  void endpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass)
  {
    vkCmdEndRenderPass(commandbuffer);
  }


  ///////////////////////// execute /////////////////////////////////////////
  void execute(VkCommandBuffer commandbuffer, VkCommandBuffer buffer)
  {
    vkCmdExecuteCommands(commandbuffer, 1, &buffer);
  }


  ///////////////////////// bind ////////////////////////////////////////////
  void bindresource(VkCommandBuffer commandbuffer, VkDescriptorSet descriptorset, VkPipelineLayout layout, uint32_t set, VkPipelineBindPoint bindpoint)
  {
    vkCmdBindDescriptorSets(commandbuffer, bindpoint, layout, set, 1, &descriptorset, 0, nullptr);
  }


  ///////////////////////// bind ////////////////////////////////////////////
  void bindresource(VkCommandBuffer commandbuffer, VkDescriptorSet descriptorset, VkPipelineLayout layout, uint32_t set, uint32_t offset, VkPipelineBindPoint bindpoint)
  {
    vkCmdBindDescriptorSets(commandbuffer, bindpoint, layout, set, 1, &descriptorset, 1, &offset);
  }


  ///////////////////////// bind ////////////////////////////////////////////
  void bindresource(VkCommandBuffer commandbuffer, VkPipeline pipeline, int x, int y, int width, int height, VkPipelineBindPoint bindpoint)
  {
    vkCmdBindPipeline(commandbuffer, bindpoint, pipeline);

    VkViewport viewport = {};
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(commandbuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = x;
    scissor.offset.y = y;
    scissor.extent.width = width;
    scissor.extent.height = height;

    vkCmdSetScissor(commandbuffer, 0, 1, &scissor);
  }


  ///////////////////////// bind ////////////////////////////////////////////
  void bindresource(VkCommandBuffer commandbuffer, VertexBuffer const &vertexbuffer)
  {
    VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(commandbuffer, 0, 1, vertexbuffer.vertices.data(), offsets);
  }


  ///////////////////////// draw ////////////////////////////////////////////
  void draw(VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t instancecount, uint32_t firstvertex, uint32_t firstinstance)
  {
    vkCmdDraw(commandbuffer, vertexcount, instancecount, firstvertex, firstinstance);
  }

} // namespace vulkan


/*

void Vulkan::pre()
{
  VkSurfaceCapabilitiesKHR surfacecapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicaldevice, surface, &surfacecapabilities);

  //
  // Depth Attachment
  //

  VkImageCreateInfo depthinfo = {};
  depthinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  depthinfo.imageType = VK_IMAGE_TYPE_2D;
  depthinfo.format = VK_FORMAT_D32_SFLOAT;
  depthinfo.extent = { surfacecapabilities.currentExtent.width, surfacecapabilities.currentExtent.height, 1 };
  depthinfo.mipLevels = 1;
  depthinfo.arrayLayers = 1;
  depthinfo.samples = VK_SAMPLE_COUNT_1_BIT;
  depthinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  depthinfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  depthinfo.flags = 0;

  VkFormatProperties depthformatproperties;
  vkGetPhysicalDeviceFormatProperties(physicaldevice, depthinfo.format, &depthformatproperties);

  if ((depthformatproperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
    throw runtime_error("Vulkan vkGetPhysicalDeviceFormatProperties failed");

  if (vkCreateImage(device, &depthinfo, nullptr, &depth) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateImage failed");

  VkMemoryRequirements depthmemoryrequirements;
  vkGetImageMemoryRequirements(device, depth, &depthmemoryrequirements);

  VkMemoryAllocateInfo memoryinfo = {};
  memoryinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryinfo.allocationSize = 0;
  memoryinfo.memoryTypeIndex = 0;
  memoryinfo.allocationSize = depthmemoryrequirements.size;

  for (uint32_t i = 0; i < 32; i++)
  {
    if ((depthmemoryrequirements.memoryTypeBits >> i) & 1)
    {
      if ((physicaldevicememoryproperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0)
      {
        memoryinfo.memoryTypeIndex = i;
        break;
      }
    }
  }

  VkDeviceMemory depthmemory;
  if (vkAllocateMemory(device, &memoryinfo, nullptr, &depthmemory) != VK_SUCCESS)
    throw runtime_error("Vulkan vkAllocateMemory failed");

  if (vkBindImageMemory(device, depth, depthmemory, 0) != VK_SUCCESS)
    throw runtime_error("Vulkan vkBindImageMemory failed");

  setimagelayout(*this, depth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });

  ///

  uint32_t imagescount = 0;
  vkGetSwapchainImagesKHR(device, swapchain, &imagescount, nullptr);

  vector<VkImageView> imageviews(imagescount);

  for (size_t i = 0; i < imagescount; ++i)
  {
    VkImageViewCreateInfo imageviewinfo = {};
    imageviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageviewinfo.format = surfaceformat.format;
    imageviewinfo.flags = 0;
    imageviewinfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    imageviewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageviewinfo.subresourceRange.baseMipLevel = 0;
    imageviewinfo.subresourceRange.levelCount = 1;
    imageviewinfo.subresourceRange.baseArrayLayer = 0;
    imageviewinfo.subresourceRange.layerCount = 1;
    imageviewinfo.image = presentimages[i];

    if (vkCreateImageView(device, &imageviewinfo, nullptr, &imageviews[i]) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateImageView failed");
  }

  VkImageViewCreateInfo depthviewinfo = {};
  depthviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  depthviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depthviewinfo.format = depthinfo.format;
  depthviewinfo.flags = 0;
  depthviewinfo.subresourceRange = {};
  depthviewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  depthviewinfo.subresourceRange.baseMipLevel = 0;
  depthviewinfo.subresourceRange.levelCount = 1;
  depthviewinfo.subresourceRange.baseArrayLayer = 0;
  depthviewinfo.subresourceRange.layerCount = 1;
  depthviewinfo.image = depth;

  VkImageView depthview;
  if (vkCreateImageView(device, &depthviewinfo, nullptr, &depthview) != VK_SUCCESS)
    throw runtime_error("Vulkan vkCreateImageView failed");

  ///

  VkCommandBufferAllocateInfo drawbufferinfo = {};
  drawbufferinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  drawbufferinfo.commandPool = commandpool;
  drawbufferinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  drawbufferinfo.commandBufferCount = 1;

  vkAllocateCommandBuffers(device, &drawbufferinfo, &drawbuffer);
}
*/


