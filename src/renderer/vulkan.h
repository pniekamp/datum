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

      Handle *data() { return &std::get<0>(m_t); }
      Deleter *deleter() { return &std::get<1>(m_t); }

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

    void operator()(VkCommandBuffer buffer) { vkFreeCommandBuffers(device, commandpool, 1, &buffer); }
  };

  using CommandBuffer = VulkanResource<VkCommandBuffer, CommandBufferDeleter>;

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
    VkDeviceMemory memory;

    void operator()(VkBuffer buffer) { vkDestroyBuffer(device, buffer, nullptr); vkFreeMemory(device, memory, nullptr); }
  };

  using Buffer = VulkanResource<VkBuffer, BufferDeleter>;

  struct MemoryUnmapper
  {
    VkDevice device;
    VkDeviceMemory memory;

    void operator()(void *data) { vkUnmapMemory(device, memory); }
  };

  using Memory = VulkanResource<void*, MemoryUnmapper>;


  ////////////////////////////// functions //////////////////////////////////

  CommandPool create_commandpool(VulkanDevice const &vulkan, VkCommandPoolCreateFlags flags);

  RenderPass create_renderpass(VulkanDevice const &vulkan, VkRenderPassCreateInfo const &createinfo);

  FrameBuffer create_framebuffer(VulkanDevice const &vulkan, VkFramebufferCreateInfo const &createinfo);

  Image create_image(VulkanDevice const &vulkan, VkImageCreateInfo const &createinfo);

  ImageView create_imageview(VulkanDevice const &vulkan, VkImageViewCreateInfo const &createinfo);

  Buffer create_buffer(VulkanDevice const &vulkan, VkBufferCreateInfo const &createinfo);

  Memory map_memory(VulkanDevice const &vulkan, Buffer &buffer, VkDeviceSize offset, VkDeviceSize size);

  Fence create_fence(VulkanDevice const &vulkan, VkFenceCreateFlags flags);

  void wait(VulkanDevice const &vulkan, VkFence fence);
  void signal(VulkanDevice const &vulkan, VkFence fence);

  CommandBuffer allocate_commandbuffer(VulkanDevice const &vulkan, VkCommandPool pool, VkCommandBufferLevel level);

  void begin(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkCommandBufferUsageFlags flags);
  void end(VulkanDevice const &vulkan, VkCommandBuffer buffer);

  void submit(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkPipelineStageFlags flags = 0);
  void submit(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkPipelineStageFlags flags, VkSemaphore waitsemaphore, VkSemaphore signalsemaphore, VkFence fence);

  void transition_aquire(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkImage image);
  void transition_present(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkImage image);

  void clear(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkImage image, lml::Color4 const &color);

  void blit(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkImage src, int sx, int sy, int sw, int sh, VkImage dst, int dx, int dy);
  void blit(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkImage src, int sx, int sy, int sw, int sh, VkImage dst, int dx, int dy, int dw, int dh, VkFilter filter);

  void blit(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkBuffer src, int sw, int sh, VkImage dst, int dx, int dy, int dw, int dh);

  void setimagelayout(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkImage image, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange);
  void setimagelayout(VulkanDevice const &vulkan, VkImage image, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageSubresourceRange subresourcerange);

} // namespace
