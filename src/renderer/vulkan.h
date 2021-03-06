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
  struct DeviceAllocator;
  struct DeviceAllocatorPtr
  {
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize size;

    operator bool() const { return (memory != 0); }
  };

  //|---------------------- VulkanDevice --------------------------------------
  //|--------------------------------------------------------------------------

  struct VulkanDevice
  {
    VkDevice device = 0;

    VkPhysicalDevice physicaldevice;
    VkPhysicalDeviceProperties physicaldeviceproperties;
    VkPhysicalDeviceMemoryProperties physicaldevicememoryproperties;

    VkQueue queue;
    uint32_t queuefamily;

    DeviceAllocator *allocator;

    operator VkDevice() const { return device; }

    ~VulkanDevice();
  };

  void initialise_vulkan_device(VulkanDevice *vulkan, VkPhysicalDevice physicaldevice, VkDevice device, VkQueue queue, uint32_t queuefamily, size_t blocksize = 0);


  //|---------------------- VulkanResource ------------------------------------
  //|--------------------------------------------------------------------------

  template<typename Handle, class Deleter>
  class VulkanResource
  {
    public:
      VulkanResource() = default;

      VulkanResource(Handle object, Deleter deleter)
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

      Handle release() { Handle handle = *this; std::get<0>(m_t) = {}; return handle; }

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

  struct SemaphoreDeleter
  {
    VkDevice device;

    void operator()(VkSemaphore semaphore) { vkDestroySemaphore(device, semaphore, nullptr); }
  };

  using Semaphore = VulkanResource<VkSemaphore, SemaphoreDeleter>;

  struct SurfaceDeleter
  {
    VkInstance instance;

    void operator()(VkSurfaceKHR surface) { vkDestroySurfaceKHR(instance, surface, nullptr); }
  };

  using Surface = VulkanResource<VkSurfaceKHR, SurfaceDeleter>;

  struct SwapchainDeleter
  {
    VkDevice device;

    void operator()(VkSwapchainKHR swapchain) { vkDeviceWaitIdle(device); vkDestroySwapchainKHR(device, swapchain, nullptr); }
  };

  using Swapchain = VulkanResource<VkSwapchainKHR, SwapchainDeleter>;

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

    void operator()(VkImage image) { vkDestroyImage(device, image, nullptr); }
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

  using Buffer = VulkanResource<VkBuffer, BufferDeleter>;

  struct MemoryDeleter
  {
    VkDevice device;

    void operator()(VkDeviceMemory memory) { vkFreeMemory(device, memory, nullptr); }
  };

  struct MemoryUnmapper
  {
    VkDevice device;

    void operator()(VkDeviceMemory memory) { vkUnmapMemory(device, memory); }
  };

  template<typename View>
  struct MemoryView
  {
    void *bits;
    VulkanResource<VkDeviceMemory, MemoryUnmapper> mapped;

    operator View*() const { return data(); }

    View *operator ->() const { return data(); }

    View *data() const { return static_cast<View*>(bits); }
  };

  struct DeviceAllocatorDeleter
  {
    DeviceAllocator *allocator;

    void operator()(DeviceAllocatorPtr memory);
  };

  struct TransferBuffer
  {
    Buffer buffer;
    VkDeviceSize size;

    VulkanResource<VkDeviceMemory, MemoryDeleter> memory;

    operator VkBuffer() const { return buffer; }
  };

  struct StorageBuffer
  {
    Buffer buffer;
    VkDeviceSize size;

    VulkanResource<DeviceAllocatorPtr, DeviceAllocatorDeleter> memory;

    operator VkBuffer() const { return buffer; }
  };

  struct VertexBuffer
  {
    Buffer vertices;
    uint32_t vertexcount, vertexsize;

    Buffer indices;
    uint32_t indexcount, indexsize;

    VulkanResource<DeviceAllocatorPtr, DeviceAllocatorDeleter> memory;

    operator VkBuffer() const { return vertices; }
  };

  using VertexAttribute = VkVertexInputAttributeDescription;

  struct Texture
  {
    Image image;
    ImageView imageview;

    VkFormat format;
    uint32_t width, height, layers, levels;

    VulkanResource<DeviceAllocatorPtr, DeviceAllocatorDeleter> memory;

    operator VkImage() const { return image; }
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

  Buffer create_buffer(VulkanDevice const &vulkan, VkBufferCreateInfo const &createinfo);

  bool create_transferbuffer(VulkanDevice const &vulkan, VkDeviceSize size, TransferBuffer *transferbuffer);

  TransferBuffer create_transferbuffer(VulkanDevice const &vulkan, VkDeviceSize size);
  TransferBuffer create_transferbuffer(VulkanDevice const &vulkan, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize &size);

  bool create_storagebuffer(VulkanDevice const &vulkan, VkDeviceSize size, StorageBuffer *storagebuffer);

  StorageBuffer create_storagebuffer(VulkanDevice const &vulkan, VkDeviceSize size);
  StorageBuffer create_storagebuffer(VulkanDevice const &vulkan, VkDeviceSize size, const void *data);

  bool create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t vertexsize, VkBufferUsageFlags usage, VertexBuffer *vertexbuffer);

  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t vertexsize);
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, const void *vertices, uint32_t vertexcount, uint32_t vertexsize);
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, const void *vertices, uint32_t vertexcount, uint32_t vertexsize);

  bool create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t vertexsize, uint32_t indexcount, uint32_t indexsize, VkBufferUsageFlags usage, VertexBuffer *vertexbuffer);

  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t vertexsize, uint32_t indexcount, uint32_t indexsize);
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, const void *vertices, uint32_t vertexcount, uint32_t vertexsize, const void *indices, uint32_t indexcount, uint32_t indexsize);
  VertexBuffer create_vertexbuffer(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, const void *vertices, uint32_t vertexcount, uint32_t vertexsize, const void *indices, uint32_t indexcount, uint32_t indexsize);

  bool create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, VkFormat format, VkImageViewType type, VkImageLayout layout, VkImageUsageFlags usage, Texture *texture);

  Texture create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, VkFormat format, VkImageViewType type, VkImageLayout layout, VkImageUsageFlags usage);

  Texture create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, VkFormat format);
  Texture create_texture(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, TransferBuffer const &transferbuffer, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, VkFormat format, const void *bits);
  Texture create_texture(VulkanDevice const &vulkan, TransferBuffer const &transferbuffer, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, VkFormat format, const void *bits);

  QueryPool create_querypool(VulkanDevice const &vulkan, VkQueryPoolCreateInfo const &createinfo);

  void retreive_querypool(VulkanDevice const &vulkan, VkQueryPool querypool, uint32_t first, uint32_t count, uint64_t *results);

  void *map_memory(VulkanDevice const &vulkan, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);

  template<typename View>
  MemoryView<View> map_memory(VulkanDevice const &vulkan, TransferBuffer const &buffer, VkDeviceSize offset, VkDeviceSize size) { return { map_memory(vulkan, buffer.memory, offset, size), { buffer.memory, { vulkan.device } } }; }

  Fence create_fence(VulkanDevice const &vulkan, VkFenceCreateFlags flags = 0);

  void wait_fence(VulkanDevice const &vulkan, VkFence fence);
  bool test_fence(VulkanDevice const &vulkan, VkFence fence);
  void signal_fence(VulkanDevice const &vulkan, VkFence fence);

  Semaphore create_semaphore(VulkanDevice const &vulkan, VkSemaphoreCreateFlags flags = 0);

  void wait_semaphore(VulkanDevice const &vulkan, VkSemaphore semaphore);
  void signal_semaphore(VulkanDevice const &vulkan, VkSemaphore semaphore);

  DescriptorSet allocate_descriptorset(VulkanDevice const &vulkan, VkDescriptorPool pool, VkDescriptorSetLayout layout);

  void bind_buffer(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkDescriptorType type);

  void bind_image(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkDescriptorImageInfo const *imageinfos, size_t count, VkDescriptorType type);
  void bind_image(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkImageView imageview, VkImageLayout layout, uint32_t index = 0);
  void bind_image(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, Texture const &texture, uint32_t index = 0);

  void bind_texture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkDescriptorImageInfo const *imageinfos, size_t count, VkDescriptorType type);
  void bind_texture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkImageView imageview, VkImageLayout layout, uint32_t index = 0);
  void bind_texture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkImageView imageview, VkSampler sampler, VkImageLayout layout, uint32_t index = 0);
  void bind_texture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, Texture const &texture, uint32_t index = 0);
  void bind_texture(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, Texture const &texture, Sampler const &sampler, uint32_t index = 0);

  void bind_attachment(VulkanDevice const &vulkan, VkDescriptorSet descriptorset, uint32_t binding, VkImageView imageview, VkImageLayout layout, uint32_t index = 0);

  void reset_descriptorpool(VulkanDevice const &vulkan, VkDescriptorPool descriptorpool);

  CommandBuffer allocate_commandbuffer(VulkanDevice const &vulkan, VkCommandPool pool, VkCommandBufferLevel level);

  void reset_commandpool(VulkanDevice const &vulkan, VkCommandPool commandpool);

  void begin(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkCommandBufferUsageFlags flags);
  void begin(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkFramebuffer framebuffer, VkRenderPass renderpass, uint32_t subpass, VkCommandBufferUsageFlags flags);
  void end(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer);

  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkSemaphore const (&dependancies)[8] = {});
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkFence fence, VkSemaphore const (&dependancies)[8] = {});
  void submit(VulkanDevice const &vulkan, VkCommandBuffer commandbuffer, VkSemaphore signalsemaphore, VkFence fence, VkSemaphore const (&dependancies)[8] = {});

  void clear(VkCommandBuffer commandbuffer, VkImage image, VkImageLayout layout, lml::Color4 const &clearcolor, VkImageSubresourceRange subresourcerange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

  void update(VkCommandBuffer commandbuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void *data);

  void barrier(VkCommandBuffer commandbuffer);
  void barrier(VkCommandBuffer commandbuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size);

  void mip(VkCommandBuffer commandbuffer, VkImage image, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels);

  void blit(VkCommandBuffer commandbuffer, VkImage src, uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh, VkImageSubresourceLayers srclayers, VkImage dst, uint32_t dx, uint32_t dy, VkImageSubresourceLayers dstlayers);
  void blit(VkCommandBuffer commandbuffer, VkImage src, uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh, VkImageSubresourceLayers srclayers, VkImage dst, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, VkImageSubresourceLayers dstlayers, VkFilter filter);
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize offset, VkImage dst, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, VkImageSubresourceLayers dstlayers);
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize offset, VkImage dst, uint32_t dx, uint32_t dy, uint32_t dz, uint32_t dw, uint32_t dh, uint32_t dd, VkImageSubresourceLayers dstlayers);
  void blit(VkCommandBuffer commandbuffer, VkImage src, uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh, VkImageSubresourceLayers srclayers, VkBuffer dst, VkDeviceSize offset);
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize srcoffset, VkBuffer dst, VkDeviceSize dstoffset, VkDeviceSize size);
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize srcoffset, Texture &texture);
  void blit(VkCommandBuffer commandbuffer, VkBuffer src, VkDeviceSize srcoffset, Texture &texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t layer, uint32_t level);

  void fill(VkCommandBuffer commandbuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, uint32_t data);

  void setimagelayout(VkCommandBuffer commandbuffer, VkImage image, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange);
  void setimagelayout(VkCommandBuffer commandbuffer, Texture const &texture, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange);
  void setimagelayout(VkCommandBuffer commandbuffer, Texture const &texture, VkImageLayout oldlayout, VkImageLayout newlayout);

  void reset_querypool(VkCommandBuffer commandbuffer, VkQueryPool querypool, uint32_t first, uint32_t count);

  void querytimestamp(VkCommandBuffer commandbuffer, VkQueryPool pool, uint32_t query);

  void beginquery(VkCommandBuffer commandbuffer, VkQueryPool pool, uint32_t query, VkQueryControlFlags flags);
  void endquery(VkCommandBuffer commandbuffer, VkQueryPool pool, uint32_t query);

  void beginpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass, VkFramebuffer framebuffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height, size_t attachments, VkClearValue const *clearvalues);
  void nextsubpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass);
  void endpass(VkCommandBuffer commandbuffer, VkRenderPass renderpass);

  void clear(VkCommandBuffer commandbuffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height, size_t attachment, lml::Color4 const &clearcolor, uint32_t baselayer = 0, uint32_t layercount = 1);
  void clear(VkCommandBuffer commandbuffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height, float depth, uint32_t stencil, uint32_t baselayer = 0, uint32_t layercount = 1);

  void scissor(VkCommandBuffer commandbuffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

  void execute(VkCommandBuffer commandbuffer, VkCommandBuffer buffer);

  void push(VkCommandBuffer commandbuffer, VkPipelineLayout layout, VkDeviceSize offset, VkDeviceSize size, const void *data, VkShaderStageFlags stage);

  void set_stencil_reference(VkCommandBuffer commandbuffer, VkStencilFaceFlags facemask, uint32_t reference);

  void bind_descriptor(VkCommandBuffer commandbuffer, VkPipelineLayout layout, uint32_t set, VkDescriptorSet descriptorset, VkPipelineBindPoint bindpoint);
  void bind_descriptor(VkCommandBuffer commandbuffer, VkPipelineLayout layout, uint32_t set, VkDescriptorSet descriptorset, uint32_t offset, VkPipelineBindPoint bindpoint);

  void bind_pipeline(VkCommandBuffer commandbuffer, VkPipeline pipeline, VkPipelineBindPoint bindpoint);
  void bind_pipeline(VkCommandBuffer commandbuffer, VkPipeline pipeline, uint32_t x, uint32_t y, uint32_t width, uint32_t height, VkPipelineBindPoint bindpoint);
  void bind_pipeline(VkCommandBuffer commandbuffer, VkPipeline pipeline, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t clipx, uint32_t clipy, uint32_t clipwidth, uint32_t clipheight, VkPipelineBindPoint bindpoint);

  void bind_vertexbuffer(VkCommandBuffer commandbuffer, uint32_t binding, VertexBuffer const &vertexbuffer);

  void draw(VkCommandBuffer commandbuffer, uint32_t vertexcount, uint32_t instancecount, uint32_t firstvertex, uint32_t firstinstance);
  void draw(VkCommandBuffer commandbuffer, uint32_t indexcount, uint32_t instancecount, uint32_t firstindex, int32_t vertexoffset, uint32_t firstinstance);

  void dispatch(VkCommandBuffer commandbuffer, uint32_t x, uint32_t y, uint32_t z);
  void dispatch(VkCommandBuffer commandbuffer, uint32_t width, uint32_t height, uint32_t depth, uint32_t const (&dim)[3]);
  void dispatch(VkCommandBuffer commandbuffer, Texture const &texture, uint32_t width, uint32_t height, uint32_t depth, uint32_t const (&dim)[3]);

} // namespace
