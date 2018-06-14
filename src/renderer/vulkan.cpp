//
// Datum - vulkan interface
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "vulkan.h"
#include <vector>
#include <atomic>
#include <algorithm>
#include "debug.h"

using namespace std;
using namespace lml;
using leap::alignto;

namespace Vulkan
{
  //|---------------------- DeviceAllocator ---------------------------------
  //|------------------------------------------------------------------------

  struct DeviceAllocator
  {
    VkDeviceSize blocksize = 0;

    struct Slab
    {
      uint32_t type;
      VkDeviceMemory memory;

      struct Block
      {
        VkDeviceSize offset;
        VkDeviceSize size;
      };

      std::vector<Block> freeblocks; // sorted by size
    };

    std::vector<Slab> slabs;

    VkDeviceSize m_used = 0;
    VkDeviceSize m_allocated = 0;

    std::atomic_flag lock = ATOMIC_FLAG_INIT;
  };

  bool less_size(DeviceAllocator::Slab::Block const &lhs, VkDeviceSize size)
  {
    return lhs.size < size;
  }


  ///////////////////////// memory_type /////////////////////////////////////
  uint32_t memory_type(VulkanDevice const &vulkan, VkMemoryRequirements const &requirements, VkMemoryPropertyFlags properties)
  {
    for (uint32_t i = 0; i < 32; i++)
    {
      if ((requirements.memoryTypeBits >> i) & 1)
      {
        if ((vulkan.physicaldevicememoryproperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
          return i;
        }
      }
    }

    return 0;
  }


  ///////////////////////// allocate_memory /////////////////////////////////
  VkResult allocate_memory(VulkanDevice const &vulkan, VkMemoryRequirements const &requirements, VkMemoryPropertyFlags properties, VkDeviceMemory *memory)
  {
    VkMemoryAllocateInfo allocateinfo = {};
    allocateinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateinfo.memoryTypeIndex = memory_type(vulkan, requirements, properties);
    allocateinfo.allocationSize = requirements.size;

    return vkAllocateMemory(vulkan.device, &allocateinfo, nullptr, memory);
  }


  ///////////////////////// deviceallocater_allocate ////////////////////////
  VkResult deviceallocater_allocate(VulkanDevice const &vulkan, VkMemoryRequirements const &requirements, VkMemoryPropertyFlags properties, DeviceAllocatorPtr *block)
  {
    block->memory = 0;
    block->offset = 0;
    block->size = requirements.size;

    assert(properties == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto allocator = vulkan.allocator;

    while (allocator->lock.test_and_set(std::memory_order_acquire))
      ;

    uint32_t type = memory_type(vulkan, requirements, properties);

    VkDeviceSize size = alignto(requirements.size, max(vulkan.physicaldeviceproperties.limits.bufferImageGranularity, VkDeviceSize(1024)));

    for(auto &slab : allocator->slabs)
    {
      if (slab.type == type)
      {
        auto first = lower_bound(begin(slab.freeblocks), end(slab.freeblocks), size, &less_size);

        for(auto &i = first; i != slab.freeblocks.end(); ++i)
        {
          VkDeviceSize offset = alignto(i->offset, requirements.alignment);

          if (offset + size <= i->offset + i->size)
          {
            block->memory = slab.memory;
            block->offset = offset;
            block->size = size;

            auto head = DeviceAllocator::Slab::Block{ i->offset, offset - i->offset };
            auto tail = DeviceAllocator::Slab::Block{ offset + size, i->size - (offset - i->offset) - size };

            slab.freeblocks.erase(i);

            if (head.size != 0)
            {
              slab.freeblocks.insert(lower_bound(begin(slab.freeblocks), end(slab.freeblocks), head.size, &less_size), head);
            }

            if (tail.size != 0)
            {
              slab.freeblocks.insert(lower_bound(begin(slab.freeblocks), end(slab.freeblocks), tail.size, &less_size), tail);
            }

            allocator->m_used += size;

            allocator->lock.clear(std::memory_order_release);

            return VK_SUCCESS;
          }
        }
      }
    }

    VkMemoryAllocateInfo allocateinfo = {};
    allocateinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateinfo.memoryTypeIndex = type;
    allocateinfo.allocationSize = max(size, allocator->blocksize);

    VkDeviceMemory memory;
    if (vkAllocateMemory(vulkan.device, &allocateinfo, nullptr, &memory) == VK_SUCCESS)
    {
      block->memory = memory;
      block->offset = 0;
      block->size = size;

      allocator->slabs.push_back({ type, memory });

      auto &slab = allocator->slabs.back();

      auto tail = DeviceAllocator::Slab::Block{ size, allocateinfo.allocationSize - size };

      if (tail.size != 0)
      {
        slab.freeblocks.insert(lower_bound(begin(slab.freeblocks), end(slab.freeblocks), tail.size, &less_size), tail);
      }

      allocator->m_used += size;
      allocator->m_allocated += allocateinfo.allocationSize;

      allocator->lock.clear(std::memory_order_release);

      return VK_SUCCESS;
    }

    allocator->lock.clear(std::memory_order_release);

    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  }


  ///////////////////////// deviceallocater_free ////////////////////////////
  void deviceallocater_free(DeviceAllocator *allocator, DeviceAllocatorPtr block)
  {
    while (allocator->lock.test_and_set(std::memory_order_acquire))
      ;

    auto slab = find_if(begin(allocator->slabs), end(allocator->slabs), [&](auto &lhs) { return (lhs.memory == block.memory); });

    assert(slab != allocator->slabs.end());

    auto free = DeviceAllocator::Slab::Block{ block.offset, block.size };

    auto i = find_if(begin(slab->freeblocks), end(slab->freeblocks), [&](auto &lhs) { return (lhs.offset + lhs.size == free.offset); });

    if (i != slab->freeblocks.end())
    {
      // Compact head

      free.offset = i->offset;
      free.size += i->size;

      slab->freeblocks.erase(i);
    }

    auto j = find_if(begin(slab->freeblocks), end(slab->freeblocks), [&](auto &lhs) { return (lhs.offset == free.offset + free.size); });

    if (j != slab->freeblocks.end())
    {
      // Compact tail

      free.size += j->size;

      slab->freeblocks.erase(j);
    }

    slab->freeblocks.insert(lower_bound(begin(slab->freeblocks), end(slab->freeblocks), free.size, &less_size), free);

    allocator->m_used -= block.size;

    allocator->lock.clear(std::memory_order_release);
  }


  void DeviceAllocatorDeleter::operator()(DeviceAllocatorPtr memory)
  {
    deviceallocater_free(allocator, memory);
  }
}

namespace Vulkan
{
  //|---------------------- VulkanDevice ------------------------------------
  //|------------------------------------------------------------------------

  ///////////////////////// initialise_vulkan_device ////////////////////////
  void initialise_vulkan_device(VulkanDevice *vulkan, VkPhysicalDevice physicaldevice, VkDevice device, VkQueue queue, uint32_t queuefamily, size_t blocksize)
  {
    vulkan->device = device;

    vulkan->physicaldevice = physicaldevice;

    vkGetPhysicalDeviceProperties(vulkan->physicaldevice, &vulkan->physicaldeviceproperties);

    vkGetPhysicalDeviceMemoryProperties(vulkan->physicaldevice, &vulkan->physicaldevicememoryproperties);

    vulkan->queue = queue;
    vulkan->queuefamily = queuefamily;

    vulkan->allocator = new DeviceAllocator;
    vulkan->allocator->blocksize = blocksize;
  }

  VulkanDevice::~VulkanDevice()
  {
    if (device != 0)
    {
      for(auto &slab : allocator->slabs)
      {
        vkFreeMemory(device, slab.memory, nullptr);
      }

      delete allocator;
    }
  }


  ///////////////////////// format_datasize /////////////////////////////////
  size_t format_datasize(uint32_t width, uint32_t height, VkFormat format)
  {
    switch(format)
    {
      case VK_FORMAT_B8G8R8A8_SRGB:
      case VK_FORMAT_B8G8R8A8_UNORM:
        return width * height * sizeof(uint32_t);

      case VK_FORMAT_BC3_SRGB_BLOCK:
      case VK_FORMAT_BC3_UNORM_BLOCK:
        return ((width + 3)/4) * ((height + 3)/4) * 16;

      case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
        return width * height * sizeof(uint32_t);

      case VK_FORMAT_R16G16B16A16_SFLOAT:
        return width * height * 4*sizeof(uint16_t);

      case VK_FORMAT_R32G32B32A32_SFLOAT:
        return width * height * 4*sizeof(uint32_t);

      case VK_FORMAT_R32_SFLOAT:
      case VK_FORMAT_D32_SFLOAT:
        return width * height * sizeof(uint32_t);

      default:
        assert(false); return 0;
    }
  }


  ///////////////////////// create_commandpool //////////////////////////////
  CommandPool create_commandpool(VulkanDevice const &vulkan, VkCommandPoolCreateFlags flags)
  {
    VkCommandPoolCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createinfo.flags = flags;
    createinfo.queueFamilyIndex = vulkan.queuefamily;

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

    return { image, { vulkan.device } };
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


  ///////////////////////// create_buffer ///////////////////////////////////
  Buffer create_buffer(VulkanDevice const &vulkan, VkBufferCreateInfo const &createinfo)
  {
    VkBuffer buffer;
    if (vkCreateBuffer(vulkan.device, &createinfo, nullptr, &buffer) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateBuffer failed");

    return { buffer, { vulkan.device } };
  }


  ///////////////////////// create_transferbuffer ///////////////////////////
  bool create_transferbuffer(VulkanDevice const &vulkan, VkDeviceSize size, TransferBuffer *transferbuffer)
  {
    VkBufferCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createinfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createinfo.size = size;

    transferbuffer->buffer = create_buffer(vulkan, createinfo);

    VkMemoryRequirements memoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, transferbuffer->buffer, &memoryrequirements);

    VkDeviceMemory memory;
    if (allocate_memory(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory) != VK_SUCCESS)
      return false;

    transferbuffer->size = size;
    transferbuffer->memory = { memory, { vulkan.device } };

    if (vkBindBufferMemory(vulkan.device, transferbuffer->buffer, memory, 0) != VK_SUCCESS)
      throw runtime_error("Vulkan vkBindBufferMemory failed");

    return true;
  }


  ///////////////////////// create_transferbuffer ///////////////////////////
  TransferBuffer create_transferbuffer(VulkanDevice const &vulkan, VkDeviceSize size)
  {
    TransferBuffer transferbuffer = {};
    if (!create_transferbuffer(vulkan, size, &transferbuffer))
      throw runtime_error("Vulkan Create TransferBuffer failed");

    return transferbuffer;
  }


  ///////////////////////// create_transferbuffer ///////////////////////////
  TransferBuffer create_transferbuffer(VulkanDevice const &vulkan, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize &size)
  {
    TransferBuffer transferbuffer = {};

    VkBufferCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createinfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createinfo.size = size;

    transferbuffer.buffer = create_buffer(vulkan, createinfo);

    VkMemoryRequirements memoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, transferbuffer.buffer, &memoryrequirements);

    if (offset % memoryrequirements.alignment != 0)
      throw runtime_error("Vulkan VkMemoryRequirements invalid alignment offset");

    if (vkBindBufferMemory(vulkan.device, transferbuffer.buffer, memory, offset) != VK_SUCCESS)
      throw runtime_error("Vulkan vkBindBufferMemory failed");

    transferbuffer.size = memoryrequirements.size;

    return transferbuffer;
  }


  ///////////////////////// create_storagebuffer /////////////////////////////
  bool create_storagebuffer(VulkanDevice const &vulkan, VkDeviceSize size, StorageBuffer *storagebuffer)
  {
    VkBufferCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createinfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createinfo.size = size;

    storagebuffer->buffer = create_buffer(vulkan, createinfo);

    VkMemoryRequirements memoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, storagebuffer->buffer, &memoryrequirements);

    DeviceAllocatorPtr block;
    if (deviceallocater_allocate(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &block) != VK_SUCCESS)
      return false;

    storagebuffer->size = size;
    storagebuffer->memory = { block, { vulkan.allocator } };

    if (vkBindBufferMemory(vulkan.device, storagebuffer->buffer, block.memory, block.offset) != VK_SUCCESS)
      throw runtime_error("Vulkan vkBindBufferMemory failed");

    return true;
  }


  ///////////////////////// create_storagebuffer /////////////////////////////
  StorageBuffer create_storagebuffer(VulkanDevice const &vulkan, VkDeviceSize size)
  {
    StorageBuffer storagebuffer = {};
    if (!create_storagebuffer(vulkan, size, &storagebuffer))
      throw runtime_error("Vulkan Create StorageBuffer failed");

    return storagebuffer;
  }


  ///////////////////////// create_storagebuffer /////////////////////////////
  StorageBuffer create_storagebuffer(VulkanDevice const &vulkan, VkDeviceSize size, const void *data)
  {
    CommandPool setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    CommandBuffer setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    StorageBuffer storagebuffer = create_storagebuffer(vulkan, size);

    fill(setupbuffer, storagebuffer, 0, size, 0);

    if (data)
    {
      for(size_t offset = 0, remaining = size; remaining > 0; )
      {
        auto bytes = min(remaining, size_t(65536));

        update(setupbuffer, storagebuffer, offset, bytes, (uint8_t const *)data + offset);

        offset += bytes;
        remaining -= bytes;
      }
    }

    end(vulkan, setupbuffer);

    submit(vulkan, setupbuffer);

    vkQueueWaitIdle(vulkan.queue);

    return storagebuffer;
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  bool create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t vertexsize, VkBufferUsageFlags usage, VertexBuffer *vertexbuffer)
  {
    VkBufferCreateInfo vertexbufferinfo = {};
    vertexbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexbufferinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;
    vertexbufferinfo.size = vertexcount * vertexsize;

    vertexbuffer->vertexcount = vertexcount;
    vertexbuffer->vertexsize = vertexsize;
    vertexbuffer->vertices = create_buffer(vulkan, vertexbufferinfo);

    VkMemoryRequirements memoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, vertexbuffer->vertices, &memoryrequirements);

    DeviceAllocatorPtr block;
    if (deviceallocater_allocate(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &block) != VK_SUCCESS)
      return false;

    vertexbuffer->memory = { block, { vulkan.allocator } };

    if (vkBindBufferMemory(vulkan.device, vertexbuffer->vertices, block.memory, block.offset) != VK_SUCCESS)
      throw runtime_error("Vulkan vkBindBufferMemory failed");

    return true;
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t vertexsize)
  {
    VertexBuffer vertexbuffer = {};
    if (!create_vertexbuffer(vulkan, commandbuffer, vertexcount, vertexsize, 0, &vertexbuffer))
      throw runtime_error("Vulkan Create VertexBuffer failed");

    return vertexbuffer;
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, const void *vertices, uint32_t vertexcount, uint32_t vertexsize)
  {
    assert(vertexcount * vertexsize <= transferbuffer.size);

    VertexBuffer vertexbuffer = create_vertexbuffer(vulkan, commandbuffer, vertexcount, vertexsize);

    VkDeviceSize verticessize = vertexbuffer.vertexcount * vertexbuffer.vertexsize;

    memcpy(map_memory<uint8_t>(vulkan, transferbuffer, 0, verticessize), vertices, verticessize);
    blit(commandbuffer, transferbuffer, 0, vertexbuffer.vertices, 0, verticessize);

    return vertexbuffer;
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, const void *vertices, uint32_t vertexcount, uint32_t vertexsize)
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
  bool create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t vertexsize, uint32_t indexcount, uint32_t indexsize, VkBufferUsageFlags usage, VertexBuffer *vertexbuffer)
  {
    VkBufferCreateInfo vertexbufferinfo = {};
    vertexbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexbufferinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertexbufferinfo.size = vertexcount * vertexsize;

    vertexbuffer->vertexcount = vertexcount;
    vertexbuffer->vertexsize = vertexsize;
    vertexbuffer->vertices = create_buffer(vulkan, vertexbufferinfo);

    VkMemoryRequirements vertexmemoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, vertexbuffer->vertices, &vertexmemoryrequirements);

    VkBufferCreateInfo indexbufferinfo = {};
    indexbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexbufferinfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    indexbufferinfo.size = indexcount * indexsize;

    vertexbuffer->indexcount = indexcount;
    vertexbuffer->indexsize = indexsize;
    vertexbuffer->indices = create_buffer(vulkan, indexbufferinfo);

    VkMemoryRequirements indexmemoryrequirements;
    vkGetBufferMemoryRequirements(vulkan.device, vertexbuffer->indices, &indexmemoryrequirements);

    VkDeviceSize padding = indexmemoryrequirements.alignment - (vertexmemoryrequirements.size % indexmemoryrequirements.alignment);

    VkMemoryRequirements memoryrequirements;
    memoryrequirements.alignment = vertexmemoryrequirements.alignment;
    memoryrequirements.memoryTypeBits = vertexmemoryrequirements.memoryTypeBits & indexmemoryrequirements.memoryTypeBits;
    memoryrequirements.size = vertexmemoryrequirements.size + padding + indexmemoryrequirements.size;

    DeviceAllocatorPtr block;
    if (deviceallocater_allocate(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &block) != VK_SUCCESS)
      return false;

    vertexbuffer->memory = { block, { vulkan.allocator } };

    if (vkBindBufferMemory(vulkan.device, vertexbuffer->vertices, block.memory, block.offset) != VK_SUCCESS)
      throw runtime_error("Vulkan vkBindBufferMemory failed");

    if (vkBindBufferMemory(vulkan.device, vertexbuffer->indices, block.memory, block.offset + vertexmemoryrequirements.size + padding) != VK_SUCCESS)
      throw runtime_error("Vulkan vkBindBufferMemory failed");

    return true;
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t vertexsize, uint32_t indexcount, uint32_t indexsize)
  {
    VertexBuffer vertexbuffer = {};
    if (!create_vertexbuffer(vulkan, commandbuffer, vertexcount, vertexsize, indexcount, indexsize, 0, &vertexbuffer))
      throw runtime_error("Vulkan Create VertexBuffer failed");

    return vertexbuffer;
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, const void *vertices, uint32_t vertexcount, uint32_t vertexsize, const void *indices, uint32_t indexcount, uint32_t indexsize)
  {
    assert(vertexcount * vertexsize + indexcount * indexsize <= transferbuffer.size);

    VertexBuffer vertexbuffer = create_vertexbuffer(vulkan, commandbuffer, vertexcount, vertexsize, indexcount, indexsize);

    VkDeviceSize verticessize = vertexbuffer.vertexcount * vertexbuffer.vertexsize;
    VkDeviceSize indicessize = vertexbuffer.indexcount * vertexbuffer.indexsize;

    memcpy(map_memory<uint8_t>(vulkan, transferbuffer, 0, verticessize), vertices, verticessize);
    blit(commandbuffer, transferbuffer, 0, vertexbuffer.vertices, 0, verticessize);

    memcpy(map_memory<uint8_t>(vulkan, transferbuffer, verticessize, indicessize), indices, indicessize);
    blit(commandbuffer, transferbuffer, verticessize, vertexbuffer.indices, 0, indicessize);

    return vertexbuffer;
  }


  ///////////////////////// create_vertexbuffer /////////////////////////////
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, const void *vertices, uint32_t vertexcount, uint32_t vertexsize, const void *indices, uint32_t indexcount, uint32_t indexsize)
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


  ///////////////////////// create_texture //////////////////////////////////
  bool create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, VkFormat format, VkImageViewType type, VkImageLayout layout, VkImageUsageFlags usage, Texture *texture)
  {
    texture->width = width;
    texture->height = height;
    texture->layers = layers;
    texture->levels = levels;
    texture->format = format;

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

    if (type == VK_IMAGE_VIEW_TYPE_3D)
    {
      texture->layers = 1;
      imageinfo.imageType = VK_IMAGE_TYPE_3D;
      imageinfo.extent.depth = layers;
      imageinfo.arrayLayers = 1;
    }

    if (type == VK_IMAGE_VIEW_TYPE_CUBE)
    {
      imageinfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    texture->image = create_image(vulkan, imageinfo);

    VkMemoryRequirements memoryrequirements;
    vkGetImageMemoryRequirements(vulkan.device, texture->image, &memoryrequirements);

    DeviceAllocatorPtr block;
    if (deviceallocater_allocate(vulkan, memoryrequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &block) != VK_SUCCESS)
      return false;

    texture->memory = { block, { vulkan.allocator } };

    if (vkBindImageMemory(vulkan.device, texture->image, block.memory, block.offset) != VK_SUCCESS)
      throw runtime_error("Vulkan vkBindImageMemory failed");

    VkImageViewCreateInfo viewinfo = {};
    viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewinfo.viewType = type;
    viewinfo.format = format;
    viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, imageinfo.mipLevels, 0, imageinfo.arrayLayers };
    viewinfo.image = texture->image;

    if (format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT)
      viewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    if (format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT)
      viewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    texture->imageview = create_imageview(vulkan, viewinfo);

    if (layout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
      setimagelayout(commandbuffer, viewinfo.image, VK_IMAGE_LAYOUT_UNDEFINED, layout, viewinfo.subresourceRange);
    }

    return true;
  }


  ///////////////////////// create_texture //////////////////////////////////
  Texture create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, VkFormat format, VkImageViewType type, VkImageLayout layout, VkImageUsageFlags usage)
  {
    Texture texture = {};
    if (!create_texture(vulkan, commandbuffer, width, height, layers, levels, format, type, layout, usage, &texture))
      throw runtime_error("Vulkan Create Texture failed");

    return texture;
  }


  ///////////////////////// create_texture //////////////////////////////////
  Texture create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, VkFormat format)
  {
    Texture texture = {};
    if (!create_texture(vulkan, commandbuffer, width, height, layers, levels, format, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &texture))
      throw runtime_error("Vulkan Create Texture failed");

    return texture;
  }


  ///////////////////////// create_texture //////////////////////////////////
  Texture create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, VkFormat format, const void *bits)
  {
    Texture texture = create_texture(vulkan, commandbuffer, width, height, layers, levels, format);

    size_t size = 0;
    for(size_t i = 0; i < texture.levels; ++i)
    {
      size += format_datasize(texture.width >> i, texture.height >> i, texture.format) * texture.layers;
    }

    assert(size <= transferbuffer.size);

    memcpy(map_memory<uint8_t>(vulkan, transferbuffer, 0, size), bits, size);

    blit(commandbuffer, transferbuffer, 0, texture);

    return texture;
  }


  ///////////////////////// create_texture //////////////////////////////////
  Texture create_texture(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, VkFormat format, const void *bits)
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
  void *map_memory(VulkanDevice const &vulkan, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size)
  {
    void *data = nullptr;
    vkMapMemory(vulkan.device, memory, offset, size, 0, &data);

    return data;
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
  void wait_fence(VulkanDevice const &vulkan, VkFence fence)
  {
    vkWaitForFences(vulkan.device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkResetFences(vulkan.device, 1, &fence);
  }


  ///////////////////////// test ////////////////////////////////////////////
  bool test_fence(VulkanDevice const &vulkan, VkFence fence)
  {
    return (vkGetFenceStatus(vulkan.device, fence) == VK_SUCCESS);
  }


  ///////////////////////// signal //////////////////////////////////////////
  void signal_fence(VulkanDevice const &vulkan, VkFence fence)
  {
    vkQueueSubmit(vulkan.queue, 0, nullptr, fence);
  }


  ///////////////////////// create_semaphore ////////////////////////////////
  Semaphore create_semaphore(VulkanDevice const &vulkan, VkSemaphoreCreateFlags flags)
  {
    VkSemaphoreCreateInfo semaphoreinfo = {};
    semaphoreinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreinfo.flags = flags;

    VkSemaphore semaphore;
    if (vkCreateSemaphore(vulkan.device, &semaphoreinfo, nullptr, &semaphore) != VK_SUCCESS)
      throw runtime_error("Vulkan vkCreateSemaphore failed");

    return { semaphore, { vulkan.device } };
  }


  ///////////////////////// wait ////////////////////////////////////////////
  void wait_semaphore(VulkanDevice const &vulkan, VkSemaphore semaphore)
  {
    static const auto waitdststagemask = VkPipelineStageFlags(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    auto fence = create_fence(vulkan);

    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.waitSemaphoreCount = 1;
    submitinfo.pWaitSemaphores = &semaphore;
    submitinfo.pWaitDstStageMask = &waitdststagemask;

    vkQueueSubmit(vulkan.queue, 1, &submitinfo, fence);

    wait_fence(vulkan, fence);
  }


  ///////////////////////// signal //////////////////////////////////////////
  void signal_semaphore(VulkanDevice const &vulkan, VkSemaphore semaphore)
  {
    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.signalSemaphoreCount = 1;
    submitinfo.pSignalSemaphores = &semaphore;

    vkQueueSubmit(vulkan.queue, 1, &submitinfo, VK_NULL_HANDLE);
  }


  ///////////////////////// allocate_descriptorset //////////////////////////
  DescriptorSet allocate_descriptorset(VulkanDevice const &vulkan, VkDescriptorPool pool, VkDescriptorSetLayout layout)
  {
    VkDescriptorSetAllocateInfo allocateinfo = {};
    allocateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateinfo.descriptorPool = pool;
    allocateinfo.descriptorSetCount = 1;
    allocateinfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorset;
    if (vkAllocateDescriptorSets(vulkan.device, &allocateinfo, &descriptorset) != VK_SUCCESS)
      throw runtime_error("Vulkan vkAllocateDescriptorSets failed");

    return { descriptorset, { vulkan.device, pool } };
  }


  ///////////////////////// bind_buffer /////////////////////////////////////
  void bind_buffer(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkDescriptorType type)
  {
    VkDescriptorBufferInfo bufferinfo = {};
    bufferinfo.buffer = buffer;
    bufferinfo.offset = offset;
    bufferinfo.range = size;

    VkWriteDescriptorSet writeset = {};
    writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeset.dstSet = descriptorset;
    writeset.dstBinding = binding;
    writeset.descriptorCount = 1;
    writeset.descriptorType = type;
    writeset.pBufferInfo = &bufferinfo;

    vkUpdateDescriptorSets(vulkan.device, 1, &writeset, 0, nullptr);
  }


  ///////////////////////// bind_image //////////////////////////////////////
  void bind_image(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkDescriptorImageInfo const *imageinfos, size_t count, VkDescriptorType type)
  {
    VkWriteDescriptorSet writeset = {};
    writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeset.dstSet = descriptorset;
    writeset.dstBinding = binding;
    writeset.descriptorCount = count;
    writeset.descriptorType = type;
    writeset.pImageInfo = imageinfos;

    vkUpdateDescriptorSets(vulkan.device, 1, &writeset, 0, nullptr);
  }

  void bind_image(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkImageView imageview, VkImageLayout layout, uint32_t index)
  {
    VkDescriptorImageInfo imageinfo = {};
    imageinfo.imageView = imageview;
    imageinfo.imageLayout = layout;

    VkWriteDescriptorSet writeset = {};
    writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeset.dstSet = descriptorset;
    writeset.dstArrayElement = index;
    writeset.dstBinding = binding;
    writeset.descriptorCount = 1;
    writeset.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeset.pImageInfo = &imageinfo;

    vkUpdateDescriptorSets(vulkan.device, 1, &writeset, 0, nullptr);
  }

  void bind_image(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, Texture const &texture, uint32_t index)
  {
    bind_image(vulkan, descriptorset, binding, texture.imageview, VK_IMAGE_LAYOUT_GENERAL, index);
  }


  ///////////////////////// bind_texture ////////////////////////////////////
  void bind_texture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkDescriptorImageInfo const *imageinfos, size_t count, VkDescriptorType type)
  {
    VkWriteDescriptorSet writeset = {};
    writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeset.dstSet = descriptorset;
    writeset.dstBinding = binding;
    writeset.descriptorCount = count;
    writeset.descriptorType = type;
    writeset.pImageInfo = imageinfos;

    vkUpdateDescriptorSets(vulkan.device, 1, &writeset, 0, nullptr);
  }

  void bind_texture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkImageView imageview, VkImageLayout layout, uint32_t index)
  {
    VkDescriptorImageInfo imageinfo = {};
    imageinfo.imageView = imageview;
    imageinfo.imageLayout = layout;

    VkWriteDescriptorSet writeset = {};
    writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeset.dstSet = descriptorset;
    writeset.dstBinding = binding;
    writeset.dstArrayElement = index;
    writeset.descriptorCount = 1;
    writeset.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writeset.pImageInfo = &imageinfo;

    vkUpdateDescriptorSets(vulkan.device, 1, &writeset, 0, nullptr);
  }

  void bind_texture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkImageView imageview, VkSampler sampler, VkImageLayout layout, uint32_t index)
  {
    VkDescriptorImageInfo imageinfo = {};
    imageinfo.sampler = sampler;
    imageinfo.imageView = imageview;
    imageinfo.imageLayout = layout;

    VkWriteDescriptorSet writeset = {};
    writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeset.dstSet = descriptorset;
    writeset.dstBinding = binding;
    writeset.dstArrayElement = index;
    writeset.descriptorCount = 1;
    writeset.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeset.pImageInfo = &imageinfo;

    vkUpdateDescriptorSets(vulkan.device, 1, &writeset, 0, nullptr);
  }

  void bind_texture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, Texture const &texture, uint32_t index)
  {
    bind_texture(vulkan, descriptorset, binding, texture.imageview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, index);
  }

  void bind_texture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, Texture const &texture, Sampler const &sampler, uint32_t index)
  {
    bind_texture(vulkan, descriptorset, binding, texture.imageview, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, index);
  }


  ///////////////////////// bind_attachment /////////////////////////////////
  void bind_attachment(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkImageView imageview, VkImageLayout layout, uint32_t index)
  {
    VkDescriptorImageInfo imageinfo = {};
    imageinfo.imageView = imageview;
    imageinfo.imageLayout = layout;

    VkWriteDescriptorSet writeset = {};
    writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeset.dstSet = descriptorset;
    writeset.dstBinding = binding;
    writeset.dstArrayElement = index;
    writeset.descriptorCount = 1;
    writeset.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    writeset.pImageInfo = &imageinfo;

    vkUpdateDescriptorSets(vulkan.device, 1, &writeset, 0, nullptr);
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

  void begin(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkFramebuffer framebuffer, VkRenderPass renderpass, uint32_t subpass, VkCommandBufferUsageFlags flags)
  {
    VkCommandBufferInheritanceInfo inheritanceinfo = {};
    inheritanceinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceinfo.framebuffer = framebuffer;
    inheritanceinfo.renderPass = renderpass;
    inheritanceinfo.subpass = subpass;

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
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkSemaphore const (&dependancies)[8])
  {
    static const auto waitdststagemask = leap::fill(VkPipelineStageFlags(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT), leap::make_index_sequence<0, 8>());

    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &commandbuffer;

    submitinfo.waitSemaphoreCount = 0;
    submitinfo.pWaitSemaphores = dependancies;
    submitinfo.pWaitDstStageMask = waitdststagemask.data();

    assert(dependancies[7] == 0);
    while (dependancies[submitinfo.waitSemaphoreCount])
      ++submitinfo.waitSemaphoreCount;

    vkQueueSubmit(vulkan.queue, 1, &submitinfo, VK_NULL_HANDLE);
  }

  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkFence fence, VkSemaphore const (&dependancies)[8])
  {
    static const auto waitdststagemask = leap::fill(VkPipelineStageFlags(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT), leap::make_index_sequence<0, 8>());

    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &commandbuffer;

    submitinfo.waitSemaphoreCount = 0;
    submitinfo.pWaitSemaphores = dependancies;
    submitinfo.pWaitDstStageMask = waitdststagemask.data();

    assert(dependancies[7] == 0);
    while (dependancies[submitinfo.waitSemaphoreCount])
      ++submitinfo.waitSemaphoreCount;

    vkQueueSubmit(vulkan.queue, 1, &submitinfo, fence);
  }

  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkSemaphore signalsemaphore, VkFence fence, VkSemaphore const (&dependancies)[8])
  {
    static const auto waitdststagemask = leap::fill(VkPipelineStageFlags(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT), leap::make_index_sequence<0, 8>());

    VkSubmitInfo submitinfo = {};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &commandbuffer;
    submitinfo.signalSemaphoreCount = 1;
    submitinfo.pSignalSemaphores = &signalsemaphore;

    submitinfo.waitSemaphoreCount = 0;
    submitinfo.pWaitSemaphores = dependancies;
    submitinfo.pWaitDstStageMask = waitdststagemask.data();

    assert(dependancies[7] == 0);
    while (dependancies[submitinfo.waitSemaphoreCount])
      ++submitinfo.waitSemaphoreCount;

    vkQueueSubmit(vulkan.queue, 1, &submitinfo, fence);
  }


  ///////////////////////// clear ///////////////////////////////////////////
  void clear(VkCommandBuffer commandbuffer, VkImage image, VkImageLayout layout, Color4 const &clearcolor, VkImageSubresourceRange subresourcerange)
  {
    VkClearColorValue clearvalues = { { clearcolor.r, clearcolor.g, clearcolor.b, clearcolor.a } };

    setimagelayout(commandbuffer, image, layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourcerange);

    vkCmdClearColorImage(commandbuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearvalues, 1, &subresourcerange);

    setimagelayout(commandbuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout, subresourcerange);
  }


  ///////////////////////// update //////////////////////////////////////////
  void update(VkCommandBuffer commandbuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void *data)
  {
    vkCmdUpdateBuffer(commandbuffer, buffer, offset, size, (uint32_t const *)data);
  }


  ///////////////////////// barrier /////////////////////////////////////////
  void barrier(VkCommandBuffer commandbuffer)
  {
    VkMemoryBarrier memorybarrier = {};
    memorybarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memorybarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    memorybarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

    vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
  }


  ///////////////////////// barrier /////////////////////////////////////////
  void barrier(VkCommandBuffer commandbuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size)
  {
    VkBufferMemoryBarrier bufferbarrier = {};
    bufferbarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferbarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    bufferbarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bufferbarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferbarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferbarrier.buffer = buffer;
    bufferbarrier.offset = offset;
    bufferbarrier.size = size;

    vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &bufferbarrier, 0, nullptr);
  }


  ///////////////////////// mip /////////////////////////////////////////////
  void mip(VkCommandBuffer commandbuffer, VkImage image, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels)
  {
    for(uint32_t level = 1; level < levels; ++level)
    {
      VkImageBlit imageblit = {};

      imageblit.srcOffsets[0].x = 0;
      imageblit.srcOffsets[0].y = 0;
      imageblit.srcOffsets[0].z = 0;
      imageblit.srcOffsets[1].x = width >> (level-1);
      imageblit.srcOffsets[1].y = height >> (level-1);
      imageblit.srcOffsets[1].z = 1;
      imageblit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, (level-1), 0, layers };

      imageblit.dstOffsets[0].x = 0;
      imageblit.dstOffsets[0].y = 0;
      imageblit.dstOffsets[0].z = 0;
      imageblit.dstOffsets[1].x = width >> level;
      imageblit.dstOffsets[1].y = height >> level;
      imageblit.dstOffsets[1].z = 1;
      imageblit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, layers };

      vkCmdBlitImage(commandbuffer, image, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, 1, &imageblit, VK_FILTER_LINEAR);
    }
  }


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkImage src, uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh, VkImageSubresourceLayers srclayers, VkImage dst, uint32_t dx, uint32_t dy, VkImageSubresourceLayers dstlayers)
  {
    VkImageCopy imagecopy = {};

    imagecopy.srcOffset.x = sx;
    imagecopy.srcOffset.y = sy;
    imagecopy.srcOffset.z = 0;
    imagecopy.srcSubresource = srclayers;

    imagecopy.dstOffset.x = dx;
    imagecopy.dstOffset.y = dy;
    imagecopy.dstOffset.z = 0;
    imagecopy.dstSubresource = dstlayers;

    imagecopy.extent.width = sw;
    imagecopy.extent.height = sh;
    imagecopy.extent.depth = 1;

    vkCmdCopyImage(commandbuffer, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imagecopy);
  }


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkImage src, uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh, VkImageSubresourceLayers srclayers, VkImage dst, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, VkImageSubresourceLayers dstlayers, VkFilter filter)
  {
    VkImageBlit imageblit = {};

    imageblit.srcOffsets[0].x = sx;
    imageblit.srcOffsets[0].y = sy;
    imageblit.srcOffsets[0].z = 0;
    imageblit.srcOffsets[1].x = sx + sw;
    imageblit.srcOffsets[1].y = sy + sh;
    imageblit.srcOffsets[1].z = 1;
    imageblit.srcSubresource = srclayers;

    imageblit.dstOffsets[0].x = dx;
    imageblit.dstOffsets[0].y = dy;
    imageblit.dstOffsets[0].z = 0;
    imageblit.dstOffsets[1].x = dx + dw;
    imageblit.dstOffsets[1].y = dy + dh;
    imageblit.dstOffsets[1].z = 1;
    imageblit.dstSubresource = dstlayers;

    vkCmdBlitImage(commandbuffer, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageblit, filter);
  }


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize offset, VkImage dst, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, VkImageSubresourceLayers dstlayers)
  {
    VkBufferImageCopy buffercopy = {};

    buffercopy.bufferOffset = offset;

    buffercopy.imageOffset.x = dx;
    buffercopy.imageOffset.y = dy;
    buffercopy.imageOffset.z = 0;
    buffercopy.imageSubresource = dstlayers;

    buffercopy.imageExtent.width = dw;
    buffercopy.imageExtent.height = dh;
    buffercopy.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(commandbuffer, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffercopy);
  }


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize offset, VkImage dst, uint32_t dx, uint32_t dy, uint32_t dz, uint32_t dw, uint32_t dh, uint32_t dd, VkImageSubresourceLayers dstlayers)
  {
    VkBufferImageCopy buffercopy = {};

    buffercopy.bufferOffset = offset;

    buffercopy.imageOffset.x = dx;
    buffercopy.imageOffset.y = dy;
    buffercopy.imageOffset.z = dz;
    buffercopy.imageSubresource = dstlayers;

    buffercopy.imageExtent.width = dw;
    buffercopy.imageExtent.height = dh;
    buffercopy.imageExtent.depth = dd;

    vkCmdCopyBufferToImage(commandbuffer, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffercopy);
  }


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkImage src, uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh, VkImageSubresourceLayers srclayers, VkBuffer dst, VkDeviceSize offset)
  {
    VkBufferImageCopy buffercopy = {};

    buffercopy.bufferOffset = offset;

    buffercopy.imageOffset.x = sx;
    buffercopy.imageOffset.y = sy;
    buffercopy.imageOffset.z = 0;
    buffercopy.imageSubresource = srclayers;

    buffercopy.imageExtent.width = sw;
    buffercopy.imageExtent.height = sh;
    buffercopy.imageExtent.depth = 1;

    vkCmdCopyImageToBuffer(commandbuffer, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, 1, &buffercopy);
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


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize srcoffset, Texture &texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t layer, uint32_t level)
  {
    setimagelayout(commandbuffer, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, layer, 1 });

    blit(commandbuffer, src, srcoffset, texture.image, x, y, width, height, { VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1 });

    setimagelayout(commandbuffer, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, layer, 1 });
  }


  ///////////////////////// blit ////////////////////////////////////////////
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize srcoffset, Texture &texture)
  {
    setimagelayout(commandbuffer, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.levels, 0, texture.layers });

    for(uint32_t level = 0; level < texture.levels; ++level)
    {
      uint32_t width = texture.width >> level;
      uint32_t height = texture.height >> level;
      uint32_t layers = texture.layers;

      blit(commandbuffer, src, srcoffset, texture.image, 0, 0, width, height, { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, layers });

      srcoffset += format_datasize(width, height, texture.format) * layers;
    }

    setimagelayout(commandbuffer, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.levels, 0, texture.layers });
  }


  ///////////////////////// setimagelayout //////////////////////////////////
  void fill(VkCommandBuffer commandbuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, uint32_t data)
  {
    vkCmdFillBuffer(commandbuffer, buffer, offset, size, data);
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

    if (oldlayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
      imagebarrier.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    if (oldlayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      imagebarrier.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    if (newlayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
      imagebarrier.dstAccessMask |= VK_ACCESS_MEMORY_READ_BIT;

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

    if (newlayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
      imagebarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);
  }

  void setimagelayout(VkCommandBuffer commandbuffer, Texture const &texture, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange)
  {
    setimagelayout(commandbuffer, texture.image, oldlayout, newlayout, subresourcerange);
  }

  void setimagelayout(VkCommandBuffer commandbuffer, Texture const &texture, VkImageLayout oldlayout, VkImageLayout newlayout)
  {
    VkImageSubresourceRange subresourcerange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.levels, 0, texture.layers };

    if (texture.format == VK_FORMAT_D16_UNORM || texture.format == VK_FORMAT_D32_SFLOAT)
      subresourcerange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    if (texture.format == VK_FORMAT_D16_UNORM_S8_UINT || texture.format == VK_FORMAT_D24_UNORM_S8_UINT || texture.format == VK_FORMAT_D32_SFLOAT_S8_UINT)
      subresourcerange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    setimagelayout(commandbuffer, texture, oldlayout, newlayout, subresourcerange);
  }


  ///////////////////////// reset_querypool /////////////////////////////////
  void reset_querypool(VkCommandBuffer commandbuffer, VkQueryPool querypool, uint32_t first, uint32_t count)
  {
    vkCmdResetQueryPool(commandbuffer, querypool, first, count);
  }


  ///////////////////////// querytimestamp //////////////////////////////////
  void querytimestamp(VkCommandBuffer commandbuffer, VkQueryPool pool, uint32_t query)
  {
    vkCmdWriteTimestamp(commandbuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, pool, query);
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
  void beginpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass, VkFramebuffer framebuffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height, size_t attachments, VkClearValue const *clearvalues)
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


  ///////////////////////// clear ///////////////////////////////////////////
  void clear(VkCommandBuffer commandbuffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height, size_t attachment, Color4 const &clearcolor, uint32_t baselayer, uint32_t layercount)
  {
    VkClearAttachment attachments = {};
    attachments.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachments.colorAttachment = attachment;
    attachments.clearValue.color = { { clearcolor.r, clearcolor.g, clearcolor.b, clearcolor.a } };

    VkClearRect clearrect = {};
    clearrect.rect.offset.x = x;
    clearrect.rect.offset.y = y;
    clearrect.rect.extent.width = width;
    clearrect.rect.extent.height = height;
    clearrect.baseArrayLayer = baselayer;
    clearrect.layerCount = layercount;

    vkCmdClearAttachments(commandbuffer, 1, &attachments, 1, &clearrect);
  }


  ///////////////////////// clear ///////////////////////////////////////////
  void clear(VkCommandBuffer commandbuffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height, float depth, uint32_t stencil, uint32_t baselayer, uint32_t layercount)
  {
    VkClearAttachment attachments = {};
    attachments.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    attachments.clearValue.depthStencil = { depth, stencil };

    VkClearRect clearrect = {};
    clearrect.rect.offset.x = x;
    clearrect.rect.offset.y = y;
    clearrect.rect.extent.width = width;
    clearrect.rect.extent.height = height;
    clearrect.baseArrayLayer = baselayer;
    clearrect.layerCount = layercount;

    vkCmdClearAttachments(commandbuffer, 1, &attachments, 1, &clearrect);
  }


  ///////////////////////// scissor /////////////////////////////////////////
  void scissor(VkCommandBuffer commandbuffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
  {
    VkRect2D scissor = {};
    scissor.offset.x = x;
    scissor.offset.y = y;
    scissor.extent.width = width;
    scissor.extent.height = height;

    vkCmdSetScissor(commandbuffer, 0, 1, &scissor);
  }


  ///////////////////////// execute /////////////////////////////////////////
  void execute(VkCommandBuffer commandbuffer, VkCommandBuffer buffer)
  {
    vkCmdExecuteCommands(commandbuffer, 1, &buffer);
  }


  ///////////////////////// push ////////////////////////////////////////////
  void push(VkCommandBuffer commandbuffer, VkPipelineLayout layout, VkDeviceSize offset, VkDeviceSize size, const void *data, VkShaderStageFlags stage)
  {
    vkCmdPushConstants(commandbuffer, layout, stage, offset, size, data);
  }


  ///////////////////////// set_stencil_reference ///////////////////////////
  void set_stencil_reference(VkCommandBuffer commandbuffer, VkStencilFaceFlags facemask, uint32_t reference)
  {
    vkCmdSetStencilReference(commandbuffer, facemask, reference);
  }


  ///////////////////////// bind ////////////////////////////////////////////
  void bind_descriptor(VkCommandBuffer commandbuffer, VkPipelineLayout layout, uint32_t set, VkDescriptorSet descriptorset, VkPipelineBindPoint bindpoint)
  {
    vkCmdBindDescriptorSets(commandbuffer, bindpoint, layout, set, 1, &descriptorset, 0, nullptr);
  }


  ///////////////////////// bind ////////////////////////////////////////////
  void bind_descriptor(VkCommandBuffer commandbuffer, VkPipelineLayout layout, uint32_t set, VkDescriptorSet descriptorset, uint32_t offset, VkPipelineBindPoint bindpoint)
  {
    vkCmdBindDescriptorSets(commandbuffer, bindpoint, layout, set, 1, &descriptorset, 1, &offset);
  }


  ///////////////////////// bind ////////////////////////////////////////////
  void bind_pipeline(VkCommandBuffer commandbuffer, VkPipeline pipeline, VkPipelineBindPoint bindpoint)
  {
    vkCmdBindPipeline(commandbuffer, bindpoint, pipeline);
  }


  ///////////////////////// bind ////////////////////////////////////////////
  void bind_pipeline(VkCommandBuffer commandbuffer, VkPipeline pipeline, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t clipx, uint32_t clipy, uint32_t clipwidth, uint32_t clipheight, VkPipelineBindPoint bindpoint)
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
    scissor.offset.x = clipx;
    scissor.offset.y = clipy;
    scissor.extent.width = clipwidth;
    scissor.extent.height = clipheight;

    vkCmdSetScissor(commandbuffer, 0, 1, &scissor);
  }


  ///////////////////////// bind ////////////////////////////////////////////
  void bind_pipeline(VkCommandBuffer commandbuffer, VkPipeline pipeline, uint32_t x, uint32_t y, uint32_t width, uint32_t height, VkPipelineBindPoint bindpoint)
  {
    bind_pipeline(commandbuffer, pipeline, x, y, width, height, x, y, width, height, bindpoint);
  }


  ///////////////////////// bind ////////////////////////////////////////////
  void bind_vertexbuffer(VkCommandBuffer commandbuffer, uint32_t binding, VertexBuffer const &vertexbuffer)
  {
    VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(commandbuffer, binding, 1, vertexbuffer.vertices.data(), offsets);

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
    vkCmdDrawIndexed(commandbuffer, indexcount, instancecount, firstindex, vertexoffset, firstinstance);
  }


  ///////////////////////// dispatch ////////////////////////////////////////
  void dispatch(VkCommandBuffer commandbuffer, uint32_t x, uint32_t y, uint32_t z)
  {
    vkCmdDispatch(commandbuffer, x, y, z);
  }


  ///////////////////////// dispatch ////////////////////////////////////////
  void dispatch(VkCommandBuffer commandbuffer, uint32_t width, uint32_t height, uint32_t depth, uint32_t const (&dim)[3])
  {
    dispatch(commandbuffer, (width + dim[0] - 1)/dim[0], (height + dim[1] - 1)/dim[1], (depth + dim[2] - 1)/dim[2]);
  }


  ///////////////////////// dispatch ////////////////////////////////////////
  void dispatch(VkCommandBuffer commandbuffer, Texture const &texture, uint32_t width, uint32_t height, uint32_t depth, uint32_t const (&dim)[3])
  {
    setimagelayout(commandbuffer, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    dispatch(commandbuffer, width, height, depth, dim);

    setimagelayout(commandbuffer, texture, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }


} // namespace vulkan
