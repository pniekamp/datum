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
      VulkanResource(Handle object = 0, Deleter deleter = Deleter())
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


  ////////////////////////////// functions //////////////////////////////////

  Fence create_fence(VulkanDevice const &vulkan, VkFenceCreateFlags flags);

  void wait(VulkanDevice const &vulkan, VkFence fence);
  void signal(VulkanDevice const &vulkan, VkFence fence);

  CommandPool create_commandpool(VulkanDevice const &vulkan, VkCommandPoolCreateFlags flags);

  CommandBuffer allocate_commandbuffer(VulkanDevice const &vulkan, VkCommandPool pool, VkCommandBufferLevel level);

  void begin(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkCommandBufferUsageFlags flags);
  void end(VulkanDevice const &vulkan, VkCommandBuffer buffer);
  void submit(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkPipelineStageFlags flags, VkSemaphore waitsemaphore, VkSemaphore signalsemaphore, VkFence fence);

  void transition_aquire(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkImage image);
  void transition_present(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkImage image);

  void clear(VulkanDevice const &vulkan, VkCommandBuffer buffer, VkImage image, lml::Color4 const &color);

} // namespace
