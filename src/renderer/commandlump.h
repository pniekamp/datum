//
// Datum - command lump
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resourcepool.h"
#include <utility>
#include <cassert>

struct RenderContext;

//|---------------------- CommandLump ---------------------------------------
//|--------------------------------------------------------------------------

class CommandLump
{
  public:

    class Descriptor
    {
      public:

        template<typename View>
        View *memory(VkDeviceSize offset = 0) const
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

        VkDeviceSize used() const { return m_used; }
        VkDeviceSize capacity() const { return m_storage.capacity; }
        VkDeviceSize available() const { return m_storage.capacity - m_used; }

        operator VkDescriptorSet() { return m_descriptor; }

      public:

        Descriptor()
        {
          m_used = 0;
          m_pool = nullptr;
          m_storage = {};
          m_descriptor = {};
        }

        Descriptor(Descriptor const &) = delete;

        Descriptor(Descriptor &&other) noexcept
          : Descriptor()
        {
          operator=(std::move(other));
        }

        ~Descriptor()
        {
          if (m_pool && m_storage)
          {
            m_pool->release_storagebuffer(m_storage, m_used);
          }
        }

        Descriptor &operator=(Descriptor &&other) noexcept
        {
          std::swap(m_used, other.m_used);
          std::swap(m_pool, other.m_pool);
          std::swap(m_storage, other.m_storage);
          std::swap(m_descriptor, other.m_descriptor);

          return *this;
        }

      private:

        VkDeviceSize m_used;

        ResourcePool *m_pool;
        ResourcePool::StorageBuffer m_storage;
        ResourcePool::DescriptorSet m_descriptor;

        friend class CommandLump;
    };

  public:
    CommandLump(RenderContext *context);
    CommandLump(CommandLump const &) = delete;
    ~CommandLump();

    VkCommandBuffer allocate_commandbuffer();

    VkDescriptorSet allocate_descriptorset(VkDescriptorSetLayout layout);

    Descriptor acquire_descriptor(VkDeviceSize required, Descriptor &&oldset = {});
    Descriptor acquire_descriptor(VkDescriptorSetLayout layout, Descriptor &&oldset = {});
    Descriptor acquire_descriptor(VkDescriptorSetLayout layout, VkDeviceSize required, Descriptor &&oldset = {});

  private:

    RenderContext *context;

    ResourcePool::ResourceLump const *m_resourcelump;
};
