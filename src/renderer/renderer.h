//
// Datum - renderer
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "datum/platform.h"
#include "datum/math.h"
#include "datum/asset.h"
#include "camera.h"
#include "vulkan.h"
#include "resourcepool.h"
#include "commandlist.h"
#include <tuple>


// Renderables
namespace Renderable
{
  enum class Type : uint16_t
  {
    Clear,
    Sprites,
  };

  using Vec2 = lml::Vec2;
  using Vec3 = lml::Vec3;
  using Vec4 = lml::Vec4;
  using Color3 = lml::Color3;
  using Color4 = lml::Color4;
  using Attenuation = lml::Attenuation;
  using Transform = lml::Transform;

  struct Sprites
  {
    static const Type type = Type::Sprites;

    CommandList const *spritelist;
  };
}


//|---------------------- PushBuffer ----------------------------------------
//|--------------------------------------------------------------------------

class PushBuffer
{
  public:

    struct Header
    {
      Renderable::Type type;
      uint16_t size;
    };

    class const_iterator : public std::iterator<std::forward_iterator_tag, Header>
    {
      public:
        explicit const_iterator(Header const *position) : m_header(position) { }

        bool operator ==(const_iterator const &that) const { return m_header == that.m_header; }
        bool operator !=(const_iterator const &that) const { return m_header != that.m_header; }

        Header const &operator *() { return *m_header; }
        Header const *operator ->() { return &*m_header; }

        iterator &operator++()
        {
          m_header = reinterpret_cast<Header const *>(reinterpret_cast<char const *>(m_header) + m_header->size);

          return *this;
        }

      private:

        Header const *m_header;
    };

  public:

    typedef StackAllocator<> allocator_type;

    PushBuffer(allocator_type const &allocator, size_t slabsize);

    PushBuffer(PushBuffer const &other) = delete;

  public:

    // Add a renderable to the push buffer
    template<typename T>
    T *push()
    {
      return reinterpret_cast<T*>(push(T::type, sizeof(T), alignof(T)));
    }

    template<typename T>
    T *push(size_t bytes)
    {
      return reinterpret_cast<T*>(push(T::type, bytes, alignof(T)));
    }

    // Iterate a pushbuffer
    const_iterator begin() const { return const_iterator(reinterpret_cast<Header*>(m_slab)); }
    const_iterator end() const { return const_iterator(reinterpret_cast<Header*>(m_tail)); }

  protected:

    void *push(Renderable::Type type, size_t size, size_t alignment);

  private:

    size_t m_slabsize;

    void *m_slab;
    void *m_tail;
};

template<typename T>
T const *renderable_cast(PushBuffer::Header const *header)
{
  assert(T::type == header->type);

  return reinterpret_cast<T const *>(((size_t)(header + 1) + alignof(T) - 1) & -alignof(T));
}



//|---------------------- Renderer ------------------------------------------
//|--------------------------------------------------------------------------

struct RenderContext
{
  RenderContext()
    : initialised(false)
  {
  }

  Vulkan::VulkanDevice device;

  Vulkan::Fence framefence;

  Vulkan::CommandPool commandpool;
  Vulkan::CommandBuffer commandbuffers[2];

  Vulkan::TransferBuffer transferbuffer;

  Vulkan::DescriptorPool descriptorpool;

  Vulkan::DescriptorSetLayout scenesetlayout;
  Vulkan::DescriptorSetLayout materialsetlayout;
  Vulkan::DescriptorSetLayout modelsetlayout;

  Vulkan::DescriptorSet sceneset;

  Vulkan::PipelineLayout pipelinelayout;

  Vulkan::PipelineCache pipelinecache;

  Vulkan::RenderPass renderpass;

  Vulkan::Image colorbuffer;
  Vulkan::ImageView colorbufferview;
  Vulkan::FrameBuffer framebuffer;

  Vulkan::VertexAttribute vertexattributes[4];

  Vulkan::VertexBuffer unitquad;

  Vulkan::Texture whitediffuse;

  Vulkan::Pipeline spritepipeline;

  int fbowidth, fboheight;

  ResourcePool resourcepool;

  bool initialised;

  size_t frame;
  lml::Vec3 camerapos;
  lml::Quaternion3f camerarot;

  lml::Matrix4f prevview;
};


struct RenderParams
{
  lml::Vec3 sundirection = { -0.57735, -0.57735, -0.57735 };
  lml::Color3 sunintensity = { 1.0f, 1.0f, 1.0f };

  float skyboxblend = 1.0;
  lml::Quaternion3f skyboxorientation = { 1.0f, 0.0f, 0.0f, 0.0f };

  float lightfalloff = 0.66;
};


// Prepare
bool prepare_render_context(DatumPlatform::PlatformInterface &platform, RenderContext &context, AssetManager *assets);

// Fallback
void render_fallback(RenderContext &context, DatumPlatform::Viewport const &viewport, void *bitmap = nullptr, int width = 0, int height = 0);

// Render
void render(RenderContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params);
