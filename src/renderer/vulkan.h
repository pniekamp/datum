//
// Datum - vulkan interface
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "datum/math.h"
#include <vulkan/vulkan.h>
#include <tuple>

namespace vulkan
{

  //|---------------------- VulkanDevice --------------------------------------
  //|--------------------------------------------------------------------------

  struct VulkanDevice
  {
    VkPhysicalDevice physicaldevice;
    VkPhysicalDeviceProperties physicaldeviceproperties;
    VkPhysicalDeviceMemoryProperties physicaldevicememoryproperties;

    VkDevice device = 0;

    VkQueue queue;

    operator VkDevice() const { return device; }
  };

  void initialise_vulkan_device(VulkanDevice *vulkan, VkPhysicalDevice physicaldevice, VkDevice device);


  //|---------------------- VulkanResource ------------------------------------
  //|--------------------------------------------------------------------------

  template<typename Handle, class Deleter>
  class VulkanResource
  {
    public:
      VulkanResource(Handle object = {}, Deleter deleter = Deleter())
        : m_t(object, deleter)
      {
      }

      VulkanResource(VulkanResource const &) = delete;

      VulkanResource(VulkanResource &&other) noexcept
        : VulkanResource()
      {
        std::swap(m_t, other.m_t);
      }

      ~VulkanResource()
      {
        if (std::get<0>(m_t))
          std::get<1>(m_t)(std::get<0>(m_t));
      }

      VulkanResource &operator=(VulkanResource &&other) noexcept
      {
        std::swap(m_t, other.m_t);

        return *this;
      }

      Handle const *data() const { return &std::get<0>(m_t); }

      operator Handle() const { return std::get<0>(m_t); }

    private:

      std::tuple<Handle, Deleter> m_t;
  };

  struct FenceDeleter
  {
    VkDevice device;

    void operator()(VkFence fence) { vkDestroyFence(device, fence, nullptr); }
  };

  using Fence = VulkanResource<VkFence, FenceDeleter>;

  struct CommandPoolDeleter
  {
    VkDevice device;

    void operator()(VkCommandPool pool) { vkDestroyCommandPool(device, pool, nullptr); }
  };

  using CommandPool = VulkanResource<VkCommandPool, CommandPoolDeleter>;

  struct CommandBufferDeleter
  {
    VkDevice device;
    VkCommandPool commandpool;

    void operator()(VkCommandBuffer commandbuffer) { vkFreeCommandBuffers(device, commandpool, 1, &commandbuffer); }
  };

  using CommandBuffer = VulkanResource<VkCommandBuffer, CommandBufferDeleter>;

  struct DescriptorSetLayoutDeleter
  {
    VkDevice device;

    void operator()(VkDescriptorSetLayout descriptorsetlayout) { vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr); }
  };

  using DescriptorSetLayout = VulkanResource<VkDescriptorSetLayout, DescriptorSetLayoutDeleter>;

  struct PipelineLayoutDeleter
  {
    VkDevice device;

    void operator()(VkPipelineLayout pipelinelayout) { vkDestroyPipelineLayout(device, pipelinelayout, nullptr); }
  };

  using PipelineLayout = VulkanResource<VkPipelineLayout, PipelineLayoutDeleter>;

  struct PipelineDeleter
  {
    VkDevice device;

    void operator()(VkPipeline pipeline) { vkDestroyPipeline(device, pipeline, nullptr); }
  };

  using Pipeline = VulkanResource<VkPipeline, PipelineDeleter>;

  struct PipelineCacheDeleter
  {
    VkDevice device;

    void operator()(VkPipelineCache pipelinecache) { vkDestroyPipelineCache(device, pipelinecache, nullptr); }
  };

  using PipelineCache = VulkanResource<VkPipelineCache, PipelineCacheDeleter>;

  struct DescriptorPoolDeleter
  {
    VkDevice device;

    void operator()(VkDescriptorPool descriptorpool) { vkDestroyDescriptorPool(device, descriptorpool, nullptr); }
  };

  using DescriptorPool = VulkanResource<VkDescriptorPool, DescriptorPoolDeleter>;

  struct DescriptorSetDeleter
  {
    VkDevice device;
    VkDescriptorPool descriptorpool;

    void operator()(VkDescriptorSet descriptorset) { vkFreeDescriptorSets(device, descriptorpool, 1, &descriptorset); }
  };

  using DescriptorSet = VulkanResource<VkDescriptorSet, DescriptorSetDeleter>;

  struct ShaderModuleDeleter
  {
    VkDevice device;

    void operator()(VkShaderModule shadermodule) { vkDestroyShaderModule(device, shadermodule, nullptr); }
  };

  using ShaderModule = VulkanResource<VkShaderModule, ShaderModuleDeleter>;

  struct RenderPassDeleter
  {
    VkDevice device;

    void operator()(VkRenderPass renderpass) { vkDestroyRenderPass(device, renderpass, nullptr); }
  };

  using RenderPass = VulkanResource<VkRenderPass, RenderPassDeleter>;

  struct FrameBufferDeleter
  {
    VkDevice device;

    void operator()(VkFramebuffer framebuffer) { vkDestroyFramebuffer(device, framebuffer, nullptr); }
  };

  using FrameBuffer = VulkanResource<VkFramebuffer, FrameBufferDeleter>;

  struct ImageDeleter
  {
    VkDevice device;
    VkDeviceMemory memory;

    void operator()(VkImage image) { vkDestroyImage(device, image, nullptr); vkFreeMemory(device, memory, nullptr); }
  };

  using Image = VulkanResource<VkImage, ImageDeleter>;

  struct ImageViewDeleter
  {
    VkDevice device;

    void operator()(VkImageView imageview) { vkDestroyImageView(device, imageview, nullptr); }
  };

  using ImageView = VulkanResource<VkImageView, ImageViewDeleter>;

  struct BufferDeleter
  {
    VkDevice device;

    void operator()(VkBuffer buffer) { vkDestroyBuffer(device, buffer, nullptr); }
  };

  struct MemoryDeleter
  {
    VkDevice device;

    void operator()(VkDeviceMemory memory) { vkFreeMemory(device, memory, nullptr); }
  };

  struct MemoryUnmapper
  {
    VkDevice device;
    VkDeviceMemory memory;

    void operator()(void *data) { vkUnmapMemory(device, memory); }
  };

  using Memory = VulkanResource<void*, MemoryUnmapper>;

  template<typename View>
  struct MemoryView
  {
    Memory memory;

    operator View() { return data(); }

    View operator ->() { return data(); }

    View data() { return static_cast<View>(*memory.data()); }
  };

  struct TransferBuffer
  {
    VkDeviceSize size;
    VulkanResource<VkBuffer, BufferDeleter> buffer;
    VulkanResource<VkDeviceMemory, MemoryDeleter> memory;

    operator VkBuffer() const { return buffer; }
  };

  struct VertexBuffer
  {
    size_t vertexcount;
    VulkanResource<VkBuffer, BufferDeleter> vertices;

    size_t indexcount;
    VulkanResource<VkBuffer, BufferDeleter> indices;

    VulkanResource<VkDeviceMemory, MemoryDeleter> memory;

    operator VkDeviceMemory() const { return memory; }
  };

  typedef VkVertexInputAttributeDescription VertexAttribute;


  ////////////////////////////// functions //////////////////////////////////

  CommandPool create_commandpool(VulkanDevice const &vulkan, VkCommandPoolCreateFlags flags);

  DescriptorSetLayout create_descriptorsetlayout(VulkanDevice const &vulkan, VkDescriptorSetLayoutCreateInfo const &createinfo);

  PipelineLayout create_pipelinelayout(VulkanDevice const &vulkan, VkPipelineLayoutCreateInfo const &createinfo);

  Pipeline create_pipeline(VulkanDevice const &vulkan, VkPipelineCache cache, VkGraphicsPipelineCreateInfo const &createinfo);

  PipelineCache create_pipelinecache(VulkanDevice const &vulkan, VkPipelineCacheCreateInfo const &createinfo);

  DescriptorPool create_descriptorpool(VulkanDevice const &vulkan, VkDescriptorPoolCreateInfo const &createinfo);

  ShaderModule create_shadermodule(VulkanDevice const &vulkan, const void *code, size_t size);

  RenderPass create_renderpass(VulkanDevice const &vulkan, VkRenderPassCreateInfo const &createinfo);

  FrameBuffer create_framebuffer(VulkanDevice const &vulkan, VkFramebufferCreateInfo const &createinfo);

  Image create_image(VulkanDevice const &vulkan, VkImageCreateInfo const &createinfo);

  ImageView create_imageview(VulkanDevice const &vulkan, VkImageViewCreateInfo const &createinfo);

  TransferBuffer create_transferbuffer(VulkanDevice const &vulkan, VkDeviceSize size);

  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, const void *vertices, size_t vertexcount, size_t vertexsize);
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, const void *vertices, size_t vertexcount, size_t vertexsize, const void *indices, size_t indexcount, size_t indexsize);

  Memory map_memory(VulkanDevice const &vulkan, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);

  template<typename View>
  MemoryView<View> map_memory(VulkanDevice const &vulkan, TransferBuffer const &buffer, VkDeviceSize offset, VkDeviceSize size) { return { map_memory(vulkan, buffer.memory, offset, size) }; }

  Fence create_fence(VulkanDevice const &vulkan, VkFenceCreateFlags flags);

  void wait(VulkanDevice const &vulkan, VkFence fence);
  void signal(VulkanDevice const &vulkan, VkFence fence);

  DescriptorSet allocate_descriptorset(VulkanDevice const &vulkan, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkDescriptorType type);

  CommandBuffer allocate_commandbuffer(VulkanDevice const &vulkan, VkCommandPool pool, VkCommandBufferLevel level);

  void begin(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkCommandBufferUsageFlags flags);
  void end(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer);

  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkPipelineStageFlags flags = 0);
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkPipelineStageFlags flags, VkSemaphore waitsemaphore, VkSemaphore signalsemaphore, VkFence fence);

  void transition_aquire(VkCommandBuffer commandbuffer, VkImage image);
  void transition_present(VkCommandBuffer commandbuffer, VkImage image);

  void clear(VkCommandBuffer commandbuffer, VkImage image, lml::Color4 const &color);

  void update(VkCommandBuffer commandbuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, void const *data);

  void blit(VkCommandBuffer commandbuffer, VkImage src, int sx, int sy, int sw, int sh, VkImage dst, int dx, int dy);
  void blit(VkCommandBuffer commandbuffer, VkImage src, int sx, int sy, int sw, int sh, VkImage dst, int dx, int dy, int dw, int dh, VkFilter filter);

  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize offset, int sw, int sh, VkImage dst, int dx, int dy, int dw, int dh);

  void setimagelayout(VkCommandBuffer commandbuffer, VkImage image, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange);
  void setimagelayout(VulkanDevice const &vulkan, VkImage image, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange);

  void beginpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass, VkFramebuffer framebuffer, int x, int y, int width, int height, lml::Color4 const &clearcolor);
  void endpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass);

  void bindresourse(VkCommandBuffer commandbuffer, VkDescriptorSet descriptorset, VkPipelineLayout layout, uint32_t set, VkPipelineBindPoint bindpoint);
  void bindresourse(VkCommandBuffer commandbuffer, VkDescriptorSet descriptorset, VkPipelineLayout layout, uint32_t set, uint32_t offset, VkPipelineBindPoint bindpoint);

  void bindresourse(VkCommandBuffer commandbuffer, VkPipeline pipeline, VkPipelineBindPoint bindpoint);

  void bindresourse(VkCommandBuffer commandbuffer, VertexBuffer const &vertexbuffer);

} // namespace
