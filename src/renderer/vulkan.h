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

namespace Vulkan
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

  void initialise_vulkan_device(VulkanDevice *vulkan, VkPhysicalDevice physicaldevice, VkDevice device, uint32_t queueinstance);


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

      Handle release() { Handle handle = *this; std::get<0>(m_t) = 0; return handle; }

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

    void operator()(VkCommandPool commandpool) { vkDestroyCommandPool(device, commandpool, nullptr); }
  };

  using CommandPool = VulkanResource<VkCommandPool, CommandPoolDeleter>;

  struct CommandBufferDeleter
  {
    VkDevice device;
    VkCommandPool commandpool;

    void operator()(VkCommandBuffer commandbuffer) { vkFreeCommandBuffers(device, commandpool, 1, &commandbuffer); }
  };

  using CommandBuffer = VulkanResource<VkCommandBuffer, CommandBufferDeleter>;

  struct QueryPoolDeleter
  {
    VkDevice device;

    void operator()(VkQueryPool querypool) { vkDestroyQueryPool(device, querypool, nullptr); }
  };

  using QueryPool = VulkanResource<VkQueryPool, QueryPoolDeleter>;

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

  struct SamplerDeleter
  {
    VkDevice device;

    void operator()(VkSampler sampler) { vkDestroySampler(device, sampler, nullptr); }
  };

  using Sampler = VulkanResource<VkSampler, SamplerDeleter>;

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

    operator View*() const { return data(); }

    View *operator ->() const { return data(); }

    View *data() const { return static_cast<View*>(*memory.data()); }
  };

  struct TransferBuffer
  {
    VkDeviceSize size;
    VkDeviceSize offset;
    VulkanResource<VkBuffer, BufferDeleter> buffer;
    VulkanResource<VkDeviceMemory, MemoryDeleter> memory;

    operator VkBuffer() const { return buffer; }
  };

  struct VertexBuffer
  {
    size_t vertexcount, vertexsize;
    VulkanResource<VkBuffer, BufferDeleter> vertices;

    size_t indexcount, indexsize;
    VulkanResource<VkBuffer, BufferDeleter> indices;

    VkDeviceSize size;
    VkDeviceSize verticiesoffset;
    VkDeviceSize indicesoffset;
    VulkanResource<VkDeviceMemory, MemoryDeleter> memory;

    operator VkDeviceMemory() const { return memory; }
  };

  typedef VkVertexInputAttributeDescription VertexAttribute;

  struct Texture
  {
    Image image;
    ImageView imageview;
    Sampler sampler;

    VkFormat format;

    uint32_t width, height, layers, levels;

    operator VkSampler() const { return sampler; }
  };


  ////////////////////////////// functions //////////////////////////////////

  CommandPool create_commandpool(VulkanDevice const &vulkan, VkCommandPoolCreateFlags flags);

  DescriptorSetLayout create_descriptorsetlayout(VulkanDevice const &vulkan, VkDescriptorSetLayoutCreateInfo const &createinfo);

  PipelineLayout create_pipelinelayout(VulkanDevice const &vulkan, VkPipelineLayoutCreateInfo const &createinfo);

  Pipeline create_pipeline(VulkanDevice const &vulkan, VkPipelineCache cache, VkGraphicsPipelineCreateInfo const &createinfo);
  Pipeline create_pipeline(VulkanDevice const &vulkan, VkPipelineCache cache, VkComputePipelineCreateInfo const &createinfo);

  PipelineCache create_pipelinecache(VulkanDevice const &vulkan, VkPipelineCacheCreateInfo const &createinfo);

  DescriptorPool create_descriptorpool(VulkanDevice const &vulkan, VkDescriptorPoolCreateInfo const &createinfo);

  ShaderModule create_shadermodule(VulkanDevice const &vulkan, const void *code, size_t size);

  RenderPass create_renderpass(VulkanDevice const &vulkan, VkRenderPassCreateInfo const &createinfo);

  FrameBuffer create_framebuffer(VulkanDevice const &vulkan, VkFramebufferCreateInfo const &createinfo);

  Image create_image(VulkanDevice const &vulkan, VkImageCreateInfo const &createinfo);

  ImageView create_imageview(VulkanDevice const &vulkan, VkImageViewCreateInfo const &createinfo);

  Sampler create_sampler(VulkanDevice const &vulkan, VkSamplerCreateInfo const &createinfo);

  TransferBuffer create_transferbuffer(VulkanDevice const &vulkan, VkDeviceSize size);
  TransferBuffer create_transferbuffer(VulkanDevice const &vulkan, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);

  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, size_t vertexcount, size_t vertexsize);
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, const void *vertices, size_t vertexcount, size_t vertexsize);
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, const void *vertices, size_t vertexcount, size_t vertexsize);

  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, size_t vertexcount, size_t vertexsize, size_t indexcount, size_t indexsize);
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, const void *vertices, size_t vertexcount, size_t vertexsize, const void *indices, size_t indexcount, size_t indexsize);
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, const void *vertices, size_t vertexcount, size_t vertexsize, const void *indices, size_t indexcount, size_t indexsize);

  void update_vertexbuffer(VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, VertexBuffer &vertexbuffer);
  void update_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, VertexBuffer &vertexbuffer, const void *vertices);
  void update_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, VertexBuffer &vertexbuffer, const void *vertices, const void *indices);

  Texture create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, unsigned int width, unsigned int height, unsigned int layers, unsigned int levels, VkFormat format);
  Texture create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, unsigned int width, unsigned int height, unsigned int layers, unsigned int levels, VkFormat format, const void *bits);
  Texture create_texture(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, unsigned int width, unsigned int height, unsigned int layers, unsigned int levels, VkFormat format, const void *bits);

  Texture create_attachment(VulkanDevice const &vulkan, unsigned int width, unsigned int height, unsigned int layers, unsigned int levels, VkFormat format, VkImageUsageFlags usage);

  void update_texture(VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, Texture &texture);
  void update_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, Texture &texture, const void *bits);

  QueryPool create_querypool(VulkanDevice const &vulkan, VkQueryPoolCreateInfo const &createinfo);

  void retreive_querypool(VulkanDevice const &vulkan, VkQueryPool querypool, uint32_t first, uint32_t count, uint64_t *results);

  Memory map_memory(VulkanDevice const &vulkan, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);

  template<typename View>
  MemoryView<View> map_memory(VulkanDevice const &vulkan, TransferBuffer const &buffer, VkDeviceSize offset, VkDeviceSize size) { return { map_memory(vulkan, buffer.memory, buffer.offset + offset, size) }; }

  Fence create_fence(VulkanDevice const &vulkan, VkFenceCreateFlags flags = 0);

  void wait(VulkanDevice const &vulkan, VkFence fence);
  bool test(VulkanDevice const &vulkan, VkFence fence);
  void signal(VulkanDevice const &vulkan, VkFence fence);

  DescriptorSet allocate_descriptorset(VulkanDevice const &vulkan, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkDescriptorType type);
  DescriptorSet allocate_descriptorset(VulkanDevice const &vulkan, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkImageView writeimage);

  void bindtexture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkImageView imageview, VkSampler sampler);
  void bindtexture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, Texture const &texture);

  void reset_descriptorpool(VulkanDevice const &vulkan, VkDescriptorPool descriptorpool);

  CommandBuffer allocate_commandbuffer(VulkanDevice const &vulkan, VkCommandPool pool, VkCommandBufferLevel level);

  void reset_commandpool(VulkanDevice const &vulkan, VkCommandPool commandpool);

  void begin(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkCommandBufferUsageFlags flags);
  void begin(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkCommandBufferInheritanceInfo const &inheritanceinfo, VkCommandBufferUsageFlags flags);
  void begin(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkFramebuffer framebuffer, VkRenderPass renderpass, uint32_t subpass, VkCommandBufferUsageFlags flags);
  void end(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer);

  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer);
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkFence fence);
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkSemaphore waitsemaphore, VkSemaphore signalsemaphore, VkFence fence);

  void transition_acquire(VkCommandBuffer commandbuffer, VkImage image);
  void transition_present(VkCommandBuffer commandbuffer, VkImage image);

  void clear(VkCommandBuffer commandbuffer, VkImage image, lml::Color4 const &clearcolor);
  void clear(VulkanDevice const &vulkan, VkImage image, lml::Color4 const &clearcolor);

  void update(VkCommandBuffer commandbuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void *data);

  void blit(VkCommandBuffer commandbuffer, VkImage src, int sx, int sy, int sw, int sh, VkImage dst, int dx, int dy);
  void blit(VkCommandBuffer commandbuffer, VkImage src, int sx, int sy, int sw, int sh, VkImage dst, int dx, int dy, int dw, int dh, VkFilter filter);

  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize offset, int sw, int sh, VkImage dst, int dx, int dy, int dw, int dh, VkImageSubresourceLayers subresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 });

  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize srcoffset, VkBuffer dst, VkDeviceSize dstoffset, VkDeviceSize size);

  void setimagelayout(VkCommandBuffer commandbuffer, VkImage image, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange);
  void setimagelayout(VulkanDevice const &vulkan, VkImage image, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange);

  void reset_querypool(VkCommandBuffer commandbuffer, VkQueryPool querypool, uint32_t first, uint32_t count);

  void querytimestamp(VkCommandBuffer commandbuffer, VkQueryPool pool, uint32_t query);

  void beginquery(VkCommandBuffer commandbuffer, VkQueryPool pool, uint32_t query, VkQueryControlFlags flags);
  void endquery(VkCommandBuffer commandbuffer, VkQueryPool pool, uint32_t query);

  void beginpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass, VkFramebuffer framebuffer, int x, int y, int width, int height, size_t attachments, VkClearValue clearvalues[]);
  void nextsubpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass);
  void endpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass);

  void execute(VkCommandBuffer commandbuffer, VkCommandBuffer buffer);

  void bindresource(VkCommandBuffer commandbuffer, VkDescriptorSet descriptorset, VkPipelineLayout layout, uint32_t set, VkPipelineBindPoint bindpoint);
  void bindresource(VkCommandBuffer commandbuffer, VkDescriptorSet descriptorset, VkPipelineLayout layout, uint32_t set, uint32_t offset, VkPipelineBindPoint bindpoint);

  void bindresource(VkCommandBuffer commandbuffer, VkPipeline pipeline, VkPipelineBindPoint bindpoint);
  void bindresource(VkCommandBuffer commandbuffer, VkPipeline pipeline, int x, int y, int width, int height, VkPipelineBindPoint bindpoint);

  void bindresource(VkCommandBuffer commandbuffer, VertexBuffer const &vertexbuffer);

  void draw(VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t instancecount, uint32_t firstvertex, uint32_t firstinstance);
  void draw(VkCommandBuffer commandbuffer, uint32_t indexcount, uint32_t instancecount, uint32_t firstindex, int32_t vertexoffset, uint32_t firstinstance);

  void dispatch(VkCommandBuffer commandbuffer, uint32_t x, uint32_t y, uint32_t z);

} // namespace
