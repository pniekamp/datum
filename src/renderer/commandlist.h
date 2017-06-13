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
        View *memory(VkDeviceSize offset = 0)
        {
          return (View*)((uint8_t*)m_storage.memory + offset);
        }

        VkDeviceSize reserve(VkDeviceSize size)
        {
          auto offset = m_used;

          assert(offset + size <= capacity());

          m_used = (m_used + size + m_storage.alignment - 1) & -m_storage.alignment;

          return offset;
        }

        VkDeviceSize used() { return m_used; }

        VkDeviceSize capacity() { return m_storage.capacity; }

        VkDeviceSize alignment() { return m_storage.alignment; }

        operator VkDescriptorSet() { return m_descriptor.descriptorset; }

      private:

        VkDeviceSize m_used;

        ResourcePool::StorageBuffer m_storage;
        ResourcePool::DescriptorSet m_descriptor;

        friend class CommandList;
    };

  public:
    CommandList(struct RenderContext *context);
    ~CommandList();

    CommandList(CommandList const &) = delete;

    CommandList(CommandList &&other) noexcept
    {
      m_passcount = 0;
      m_resourcelump = 0;
      context = nullptr;

      *this = std::move(other);
    }

    CommandList &operator=(CommandList &&other) noexcept
    {
      std::swap(m_passcount, other.m_passcount);
      std::swap(m_commandbuffers, other.m_commandbuffers);
      std::swap(m_resourcelump, other.m_resourcelump);
      std::swap(context, other.context);

      return *this;
    }

    bool begin(VkFramebuffer framebuffer, VkRenderPass renderpass, uint32_t subpasses);

    Descriptor acquire(VkDescriptorSetLayout layout, VkDeviceSize size, Descriptor const &oldset = {});

    void release(Descriptor const &descriptor);

    void end();

  public:

    operator bool() const { return m_passcount != 0; }

    VkCommandBuffer commandbuffer(size_t subpass) const { return m_commandbuffers[subpass]; }

  private:

    size_t m_passcount;
    VkCommandBuffer m_commandbuffers[8];

    ResourcePool::ResourceLump const *m_resourcelump;

    struct RenderContext *context;
};
