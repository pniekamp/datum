//
// Datum - command list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resourcepool.h"
#include <utility>
#include <cassert>

//|---------------------- CommandList ---------------------------------------
//|--------------------------------------------------------------------------

class CommandList
{
  public:

    class Descriptor
    {
      public:

        template<typename View>
        View *memory(VkDeviceSize offset) { return (View*)((uint8_t*)m_storage.memory + offset); }

        VkDeviceSize capacity() { return m_storage.capacity; }

        VkDeviceSize alignment() { return m_storage.alignment; }

        operator VkDescriptorSet() { return m_descriptor.descriptorset; }

      private:

        ResourcePool::StorageBuffer m_storage;
        ResourcePool::DescriptorSet m_descriptor;

        friend class CommandList;
    };

  public:
    CommandList();
    ~CommandList();

    CommandList(CommandList const &) = delete;

    CommandList(CommandList &&other) noexcept
      : CommandList()
    {
      *this = std::move(other);
    }

    CommandList &operator=(CommandList &&other) noexcept
    {
      std::swap(m_commandbuffer, other.m_commandbuffer);
      std::swap(m_resourcelump, other.m_resourcelump);
      std::swap(context, other.context);

      return *this;
    }

    bool begin(class RenderContext &context, VkFramebuffer framebuffer, VkRenderPass renderpass, uint32_t subpass, size_t transferreservation);

    Descriptor acquire(VkDescriptorSetLayout layout, VkDeviceSize size);

    void release(Descriptor const &descriptor, VkDeviceSize used);

    void end();

  public:

    VkDeviceSize transferoffset;

    operator VkCommandBuffer() const { return m_commandbuffer; }

  private:

    VkCommandBuffer m_commandbuffer;

    ResourcePool::ResourceLump const *m_resourcelump;

    class RenderContext *context;
};
