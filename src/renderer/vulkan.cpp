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
  void initialise_vulkan_device(VulkanDevice *vulkan, VkPhysicalDevice physicaldevice, VkDevice device, uint32_t queueinstance)
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
    while (queueindex < queuecount && !(queueproperties[queueindex].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)))
      ++queueindex;

    if (queueproperties[queueindex].queueCount <= queueinstance)
      throw runtime_error("Vulkan vkGetDeviceQueue insufficient or incompatible queues");

    vkGetDeviceQueue(vulkan->device, queueindex, queueinstance, &vulkan->queue);
  }


  ///////////////////////// format_datasize /////////////////////////////////
  size_t format_datasize(int width, int height, VkFormat format)
  {
    switch(format)
    {
      case VK_FORMAT_B8G8R8A8_SRGB:
      case VK_FORMAT_B8G8R8A8_UNORM:
        return width * height * sizeof(uint32_t);

      case VK_FORMAT_BC3_SRGB_BLOCK:
      case VK_FORMAT_BC3_UNORM_BLOCK:
        return ((width + 3)/4) * ((height + 3)/4) * 16;

      default:
        assert(false); return 0;
    }
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


  ///////////////////////// create_pipeline /////////////////////////////////
  Pipeline create_pipeline(VulkanDevice const &vulkan, VkPipelineCache cache, VkComputePipelineCreateInfo const &createinfo)
  {
    VkPipeline pipeline;

    if (vkCreateComputePipelines(vulkan.device, cache, 1, &createinfo, nullptr, &pipeline) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateComputePipelines failed");

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


  ///////////////////////// create_sampler //////////////////////////////////
  Sampler create_sampler(VulkanDevice const &vulkan, VkSamplerCreateInfo const &createinfo)
  {
    VkSampler sampler;

    if (vkCreateSampler(vulkan.device, &createinfo, nullptr, &sampler) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateImageView failed");

    return { sampler, { vulkan.device } };
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

    return { size, 0, { transferbuffer, { vulkan.device } }, { memory, { vulkan.device } } };
  }


  ///////////////////////// create_transferbuffer ///////////////////////////
  TransferBuffer create_transferbuffer(VulkanDevice const &vulkan, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size)
  {
    VkBufferCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createinfo.size = size;

    VkBuffer transferbuffer;
    vkCreateBuffer(vulkan.device, &createinfo, nullptr, &transferbuffer);

    vkBindBufferMemory(vulkan.device, transferbuffer, memory, offset);

    return { size, offset, { transferbuffer, { vulkan.device } } };
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, size_t vertexcount, size_t vertexsize)
  {
    VkBufferCreateInfo vertexbufferinfo = {};
    vertexbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexbufferinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertexbufferinfo.size = vertexcount * vertexsize;

    VkBuffer vertexbuffer;
    vkCreateBuffer(vulkan.device, &vertexbufferinfo, nullptr, &vertexbuffer);

    VkMemoryRequirements memoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, vertexbuffer, &memoryrequirements);

    VkDeviceMemory memory;
    allocate_memory(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory);

    vkBindBufferMemory(vulkan.device, vertexbuffer, memory, 0);

    return { vertexcount, vertexsize, { vertexbuffer, { vulkan.device } }, 0, 0, { }, memoryrequirements.size, 0, 0, { memory, { vulkan.device } } };
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, const void *vertices, size_t vertexcount, size_t vertexsize)
  {
    VertexBuffer vertexbuffer = create_vertexbuffer(vulkan, commandbuffer, vertexcount, vertexsize);

    update_vertexbuffer(vulkan, commandbuffer, transferbuffer, vertexbuffer, vertices);

    return vertexbuffer;
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, const void *vertices, size_t vertexcount, size_t vertexsize)
  {
    CommandPool setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    CommandBuffer setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VertexBuffer vertexbuffer = create_vertexbuffer(vulkan, setupbuffer, transferbuffer, vertices, vertexcount, vertexsize);

    end(vulkan, setupbuffer);

    submit(vulkan, setupbuffer);

    vkQueueWaitIdle(vulkan.queue);

    return vertexbuffer;
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, size_t vertexcount, size_t vertexsize, size_t indexcount, size_t indexsize)
  {
    VkBufferCreateInfo vertexbufferinfo = {};
    vertexbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexbufferinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertexbufferinfo.size = vertexcount * vertexsize;

    VkBuffer vertexbuffer;
    vkCreateBuffer(vulkan.device, &vertexbufferinfo, nullptr, &vertexbuffer);

    VkMemoryRequirements vertexmemoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, vertexbuffer, &vertexmemoryrequirements);

    VkBufferCreateInfo indexbufferinfo = {};
    indexbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexbufferinfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    indexbufferinfo.size = indexcount * indexsize;

    VkBuffer indexbuffer;
    vkCreateBuffer(vulkan.device, &indexbufferinfo, nullptr, &indexbuffer);

    VkMemoryRequirements indexmemoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, indexbuffer, &indexmemoryrequirements);

    VkDeviceSize padding = indexmemoryrequirements.alignment - (vertexmemoryrequirements.size % indexmemoryrequirements.alignment);

    VkDeviceSize verticesoffset = 0;
    VkDeviceSize indicesoffset = verticesoffset + vertexmemoryrequirements.size + padding;

    VkMemoryRequirements memoryrequirements;
    memoryrequirements.alignment = vertexmemoryrequirements.alignment;
    memoryrequirements.memoryTypeBits = vertexmemoryrequirements.memoryTypeBits & indexmemoryrequirements.memoryTypeBits;
    memoryrequirements.size = vertexmemoryrequirements.size + padding + indexmemoryrequirements.size;

    VkDeviceMemory memory;
    allocate_memory(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory);

    vkBindBufferMemory(vulkan.device, vertexbuffer, memory, verticesoffset);
    vkBindBufferMemory(vulkan.device, indexbuffer, memory, indicesoffset);

    return { vertexcount, vertexsize, { vertexbuffer, { vulkan.device } }, indexcount, indexsize, { indexbuffer, { vulkan.device } }, memoryrequirements.size, verticesoffset, indicesoffset, { memory, { vulkan.device } } };
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, const void *vertices, size_t vertexcount, size_t vertexsize, const void *indices, size_t indexcount, size_t indexsize)
  {
    VertexBuffer vertexbuffer = create_vertexbuffer(vulkan, commandbuffer, vertexcount, vertexsize, indexcount, indexsize);

    update_vertexbuffer(vulkan, commandbuffer, transferbuffer, vertexbuffer, vertices, indices);

    return vertexbuffer;
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, const void *vertices, size_t vertexcount, size_t vertexsize, const void *indices, size_t indexcount, size_t indexsize)
  {
    CommandPool setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    CommandBuffer setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VertexBuffer vertexbuffer = create_vertexbuffer(vulkan, setupbuffer, transferbuffer, vertices, vertexcount, vertexsize, indices, indexcount, indexsize);

    end(vulkan, setupbuffer);

    submit(vulkan, setupbuffer);

    vkQueueWaitIdle(vulkan.queue);

    return vertexbuffer;
  }


  ///////////////////////// update_vertexbuffer /////////////////////////////
  void update_vertexbuffer(VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, VertexBuffer &vertexbuffer)
  {
    blit(commandbuffer, transferbuffer, vertexbuffer.verticiesoffset, vertexbuffer.vertices, 0, vertexbuffer.vertexcount * vertexbuffer.vertexsize);
    blit(commandbuffer, transferbuffer, vertexbuffer.indicesoffset, vertexbuffer.indices, 0, vertexbuffer.indexcount * vertexbuffer.indexsize);
  }


  ///////////////////////// update_vertexbuffer /////////////////////////////
  void update_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, VertexBuffer &vertexbuffer, const void *vertices)
  {
    assert(vertexbuffer.size <= transferbuffer.size);

    VkDeviceSize verticessize = vertexbuffer.vertexcount * vertexbuffer.vertexsize;

    memcpy(map_memory<uint8_t>(vulkan, transferbuffer, 0, verticessize), vertices, verticessize);

    blit(commandbuffer, transferbuffer, 0, vertexbuffer.vertices, vertexbuffer.verticiesoffset, vertexbuffer.vertexcount * vertexbuffer.vertexsize);
  }


  ///////////////////////// update_vertexbuffer /////////////////////////////
  void update_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, VertexBuffer &vertexbuffer, const void *vertices, const void *indices)
  {
    assert(vertexbuffer.size <= transferbuffer.size);

    VkDeviceSize verticessize = vertexbuffer.vertexcount * vertexbuffer.vertexsize;
    VkDeviceSize indicessize = vertexbuffer.indexcount * vertexbuffer.indexsize;

    memcpy(map_memory<uint8_t>(vulkan, transferbuffer, vertexbuffer.verticiesoffset, verticessize), vertices, verticessize);
    memcpy(map_memory<uint8_t>(vulkan, transferbuffer, vertexbuffer.indicesoffset, indicessize), indices, indicessize);

    blit(commandbuffer, transferbuffer, vertexbuffer.verticiesoffset, vertexbuffer.vertices, 0, vertexbuffer.vertexcount * vertexbuffer.vertexsize);
    blit(commandbuffer, transferbuffer, vertexbuffer.indicesoffset, vertexbuffer.indices, 0, vertexbuffer.indexcount * vertexbuffer.indexsize);
  }


  ///////////////////////// create_texture //////////////////////////////////
  Texture create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, unsigned int width, unsigned int height, unsigned int layers, unsigned int levels, VkFormat format)
  {
    Texture texture = {};

    texture.width = width;
    texture.height = height;
    texture.layers = layers;
    texture.levels = levels;
    texture.format = format;

    VkImageCreateInfo imageinfo = {};
    imageinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageinfo.imageType = VK_IMAGE_TYPE_2D;
    imageinfo.format = format;
    imageinfo.extent.width = width;
    imageinfo.extent.height = height;
    imageinfo.extent.depth = 1;
    imageinfo.mipLevels = levels;
    imageinfo.arrayLayers = layers;
    imageinfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageinfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageinfo.flags = 0;

    texture.image = create_image(vulkan, imageinfo);

    VkSamplerCreateInfo samplerinfo = {};
    samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerinfo.magFilter = VK_FILTER_LINEAR;
    samplerinfo.minFilter = VK_FILTER_LINEAR;
    samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerinfo.mipLodBias = 0.0f;
    samplerinfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerinfo.minLod = 0.0f;
    samplerinfo.maxLod = levels;
    samplerinfo.maxAnisotropy = 8;
    samplerinfo.anisotropyEnable = VK_TRUE;
    samplerinfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    texture.sampler = create_sampler(vulkan, samplerinfo);

    VkImageViewCreateInfo viewinfo = {};
    viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewinfo.format = format;
    viewinfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, layers };
    viewinfo.image = texture.image;

    texture.imageview = create_imageview(vulkan, viewinfo);

    setimagelayout(commandbuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.levels, 0, texture.layers });

    return texture;
  }


  ///////////////////////// create_texture //////////////////////////////////
  Texture create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, unsigned int width, unsigned int height, unsigned int layers, unsigned int levels, VkFormat format, const void *bits)
  {
    Texture texture = create_texture(vulkan, commandbuffer, width, height, layers, levels, format);

    update_texture(vulkan, commandbuffer, transferbuffer, texture, bits);

    return texture;
  }


  ///////////////////////// create_texture //////////////////////////////////
  Texture create_texture(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, unsigned int width, unsigned int height, unsigned int layers, unsigned int levels, VkFormat format, const void *bits)
  {
    CommandPool setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    CommandBuffer setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    Texture texture = create_texture(vulkan, setupbuffer, transferbuffer, width, height, layers, levels, format, bits);

    end(vulkan, setupbuffer);

    submit(vulkan, setupbuffer);

    vkQueueWaitIdle(vulkan.queue);

    return texture;
  }


  ///////////////////////// create_attachment ///////////////////////////////
  Texture create_attachment(VulkanDevice const &vulkan, unsigned int width, unsigned int height, unsigned int layers, unsigned int levels, VkFormat format, VkImageUsageFlags usage)
  {
    Texture texture = {};

    texture.width = width;
    texture.height = height;
    texture.layers = layers;
    texture.levels = levels;
    texture.format = format;

    VkImageCreateInfo imageinfo = {};
    imageinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageinfo.imageType = VK_IMAGE_TYPE_2D;
    imageinfo.format = format;
    imageinfo.extent.width = width;
    imageinfo.extent.height = height;
    imageinfo.extent.depth = 1;
    imageinfo.mipLevels = levels;
    imageinfo.arrayLayers = layers;
    imageinfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageinfo.usage = usage;
    imageinfo.flags = 0;

    texture.image = create_image(vulkan, imageinfo);

    VkSamplerCreateInfo samplerinfo = {};
    samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerinfo.magFilter = VK_FILTER_NEAREST;
    samplerinfo.minFilter = VK_FILTER_NEAREST;
    samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerinfo.mipLodBias = 0.0f;
    samplerinfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerinfo.minLod = 0.0f;
    samplerinfo.maxLod = levels;
    samplerinfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    texture.sampler = create_sampler(vulkan, samplerinfo);

    VkImageViewCreateInfo viewinfo = {};
    viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewinfo.format = format;
    viewinfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewinfo.subresourceRange = { (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT, 0, levels, 0, layers };
    viewinfo.image = texture.image;

    texture.imageview = create_imageview(vulkan, viewinfo);

    return texture;
  }


  ///////////////////////// update_texture //////////////////////////////////
  void update_texture(VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, Texture &texture)
  {
    setimagelayout(commandbuffer, texture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.levels, 0, texture.layers });

    size_t offset = 0;
    for(uint32_t level = 0; level < texture.levels; ++level)
    {
      uint32_t width = texture.width >> level;
      uint32_t height = texture.height >> level;
      uint32_t layers = texture.layers;

      blit(commandbuffer, transferbuffer, offset, width, height, texture.image, 0, 0, width, height, { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, layers });

      offset += format_datasize(width, height, texture.format) * layers;
    }

    setimagelayout(commandbuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.levels, 0, texture.layers });
  }


  ///////////////////////// update_texture //////////////////////////////////
  void update_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, Texture &texture, const void *bits)
  {
    size_t size = 0;
    for(size_t i = 0; i < texture.levels; ++i)
    {
      size += format_datasize(texture.width >> i, texture.height >> i, texture.format) * texture.layers;
    }

    assert(size <= transferbuffer.size);

    memcpy(map_memory<uint8_t>(vulkan, transferbuffer, 0, size), bits, size);

    update_texture(commandbuffer, transferbuffer, texture);
  }


  ///////////////////////// create_querypool ////////////////////////////////
  QueryPool create_querypool(VulkanDevice const &vulkan, VkQueryPoolCreateInfo const &createinfo)
  {
    VkQueryPool querypool;
    if (vkCreateQueryPool(vulkan.device, &createinfo, nullptr, &querypool) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateQueryPool failed");

    if (createinfo.queryType == VK_QUERY_TYPE_TIMESTAMP)
    {
      CommandPool setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

      CommandBuffer setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

      begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

      for(size_t i = 0; i < createinfo.queryCount; ++i)
        querytimestamp(setupbuffer, querypool, i);

      end(vulkan, setupbuffer);

      submit(vulkan, setupbuffer);

      vkQueueWaitIdle(vulkan.queue);
    }

    return { querypool, { vulkan.device } };
  }


  ///////////////////////// retreive_querypool //////////////////////////////
  void retreive_querypool(VulkanDevice const &vulkan, VkQueryPool querypool, uint32_t first, uint32_t count, uint64_t *results)
  {
    vkGetQueryPoolResults(vulkan.device, querypool, first, count, count*sizeof(uint64_t), results, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
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


  ///////////////////////// test ////////////////////////////////////////////
  bool test(VulkanDevice const &vulkan, VkFence fence)
  {
    return (vkGetFenceStatus(vulkan.device, fence) == VK_SUCCESS);
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


  ///////////////////////// allocate_descriptorset //////////////////////////
  DescriptorSet allocate_descriptorset(VulkanDevice const &vulkan, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkImageView writeimage)
  {
    VkDescriptorSetAllocateInfo allocateinfo = {};
    allocateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateinfo.descriptorPool = pool;
    allocateinfo.descriptorSetCount = 1;
    allocateinfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorset;
    if (vkAllocateDescriptorSets(vulkan.device, &allocateinfo, &descriptorset) != VK_SUCCESS)
      throw runtime_error("Vulkan vkAllocateDescriptorSets failed");

    VkDescriptorImageInfo imageinfo = {};
    imageinfo.imageView = writeimage;
    imageinfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writeset = {};
    writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeset.dstSet = descriptorset;
    writeset.dstBinding = 0;
    writeset.descriptorCount = 1;
    writeset.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeset.pImageInfo = &imageinfo;

    vkUpdateDescriptorSets(vulkan.device, 1, &writeset, 0, nullptr);

    return { descriptorset, { vulkan.device, pool } };
  }


  ///////////////////////// updatetexture ///////////////////////////////////
  void bindtexture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkImageView imageview, VkSampler sampler)
  {
    VkDescriptorImageInfo imageinfo = {};
    imageinfo.sampler = sampler;
    imageinfo.imageView = imageview;
    imageinfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writeset = {};
    writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeset.dstSet = descriptorset;
    writeset.dstBinding = binding;
    writeset.descriptorCount = 1;
    writeset.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeset.pImageInfo = &imageinfo;

    vkUpdateDescriptorSets(vulkan.device, 1, &writeset, 0, nullptr);
  }


  ///////////////////////// updatetexture ///////////////////////////////////
  void bindtexture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, Texture const &texture)
  {
    bindtexture(vulkan, descriptorset, binding, texture.imageview, texture.sampler);
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
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer)
  {
    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &commandbuffer;

    vkQueueSubmit(vulkan.queue, 1, &submitinfo, VK_NULL_HANDLE);
  }

  ///////////////////////// submit //////////////////////////////////////////
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkFence fence)
  {
    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &commandbuffer;

    vkQueueSubmit(vulkan.queue, 1, &submitinfo, fence);
  }

  ///////////////////////// submit //////////////////////////////////////////
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkSemaphore waitsemaphore, VkSemaphore signalsemaphore, VkFence fence)
  {
    VkPipelineStageFlags waitdststagemask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.pWaitDstStageMask = &waitdststagemask;
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
  void clear(VkCommandBuffer commandbuffer, VkImage image, Color4 const &clearcolor)
  {
    VkClearColorValue clearvalues = { clearcolor.r, clearcolor.g, clearcolor.b, clearcolor.a };

    VkImageSubresourceRange subresourcerange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdClearColorImage(commandbuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearvalues, 1, &subresourcerange);
  }


  ///////////////////////// clear ///////////////////////////////////////////
  void clear(VulkanDevice const &vulkan, VkImage image, Color4 const &clearcolor)
  {
    CommandPool setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    CommandBuffer setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    clear(setupbuffer, image, clearcolor);

    end(vulkan, setupbuffer);

    submit(vulkan, setupbuffer);

    vkQueueWaitIdle(vulkan.queue);
  }


  ///////////////////////// update //////////////////////////////////////////
  void update(VkCommandBuffer commandbuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void *data)
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
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize offset, int sw, int sh, VkImage dst, int dx, int dy, int dw, int dh, VkImageSubresourceLayers subresource)
  {
    VkBufferImageCopy buffercopy = {};

    buffercopy.bufferOffset = offset;
    buffercopy.bufferRowLength = sw;
    buffercopy.bufferImageHeight = sh;

    buffercopy.imageOffset.x = dx;
    buffercopy.imageOffset.y = dy;
    buffercopy.imageOffset.z = 1;
    buffercopy.imageSubresource = subresource;

    buffercopy.imageExtent.width = dw;
    buffercopy.imageExtent.height = dh;
    buffercopy.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(commandbuffer, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffercopy);
  }


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize srcoffset, VkBuffer dst, VkDeviceSize dstoffset, VkDeviceSize size)
  {
    VkBufferCopy buffercopy = {};

    buffercopy.srcOffset = srcoffset;
    buffercopy.dstOffset = dstoffset;
    buffercopy.size = size;

    vkCmdCopyBuffer(commandbuffer, src, dst, 1, &buffercopy);
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

    if (oldlayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
      imagebarrier.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;

    if (oldlayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
      imagebarrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;

    if (oldlayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      imagebarrier.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (oldlayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
      imagebarrier.srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;

    if (newlayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
      imagebarrier.dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;

    if (newlayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
      imagebarrier.dstAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;

    if (newlayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      imagebarrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (newlayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      imagebarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    if (newlayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
      imagebarrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;

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


  ///////////////////////// reset_querypool /////////////////////////////////
  void reset_querypool(VkCommandBuffer commandbuffer, VkQueryPool querypool, uint32_t first, uint32_t count)
  {
    vkCmdResetQueryPool(commandbuffer, querypool, first, count);
  }


  ///////////////////////// querytimestamp //////////////////////////////////
  void querytimestamp(VkCommandBuffer commandbuffer, VkQueryPool pool, uint32_t query)
  {
    vkCmdWriteTimestamp(commandbuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, pool, query);
  }


  ///////////////////////// beginquery //////////////////////////////////////
  void beginquery(VkCommandBuffer commandbuffer, VkQueryPool pool, uint32_t query, VkQueryControlFlags flags)
  {
    vkCmdBeginQuery(commandbuffer, pool, query, flags);
  }


  ///////////////////////// endquery ////////////////////////////////////////
  void endquery(VkCommandBuffer commandbuffer, VkQueryPool pool, uint32_t query)
  {
    vkCmdEndQuery(commandbuffer, pool, query);
  }


  ///////////////////////// beginpass ///////////////////////////////////////
  void beginpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass, VkFramebuffer framebuffer, int x, int y, int width, int height, size_t attachments, VkClearValue clearvalues[])
  {
    VkRenderPassBeginInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassinfo.renderPass = renderpass;
    renderpassinfo.framebuffer = framebuffer;
    renderpassinfo.renderArea.offset.x = x;
    renderpassinfo.renderArea.offset.y = y;
    renderpassinfo.renderArea.extent.width = width;
    renderpassinfo.renderArea.extent.height = height;
    renderpassinfo.clearValueCount = attachments;
    renderpassinfo.pClearValues = clearvalues;

    vkCmdBeginRenderPass(commandbuffer, &renderpassinfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
  }


  ///////////////////////// nextsubpass /////////////////////////////////////
  void nextsubpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass)
  {
    vkCmdNextSubpass(commandbuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
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
  void bindresource(VkCommandBuffer commandbuffer, VkPipeline pipeline, VkPipelineBindPoint bindpoint)
  {
    vkCmdBindPipeline(commandbuffer, bindpoint, pipeline);
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

    switch(vertexbuffer.indexsize)
    {
      case 0:
        break;

      case 2:
        vkCmdBindIndexBuffer(commandbuffer, vertexbuffer.indices, 0, VK_INDEX_TYPE_UINT16);
        break;

      case 4:
        vkCmdBindIndexBuffer(commandbuffer, vertexbuffer.indices, 0, VK_INDEX_TYPE_UINT32);
        break;
    }
  }


  ///////////////////////// draw ////////////////////////////////////////////
  void draw(VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t instancecount, uint32_t firstvertex, uint32_t firstinstance)
  {
    vkCmdDraw(commandbuffer, vertexcount, instancecount, firstvertex, firstinstance);
  }


  ///////////////////////// draw ////////////////////////////////////////////
  void draw(VkCommandBuffer commandbuffer, uint32_t indexcount, uint32_t instancecount, uint32_t firstindex, int32_t vertexoffset, uint32_t firstinstance)
  {
    vkCmdDrawIndexed(commandbuffer, indexcount, instancecount,firstindex, vertexoffset, firstinstance);
  }


  ///////////////////////// dispatch ////////////////////////////////////////
  void dispatch(VkCommandBuffer commandbuffer, uint32_t x, uint32_t y, uint32_t z)
  {
    vkCmdDispatch(commandbuffer, x, y, z);
  }

} // namespace vulkan
