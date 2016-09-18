//
// Datum - renderer
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "datum.h"
#include "datum/math.h"
#include "datum/asset.h"
#include "camera.h"
#include "vulkan.h"
#include "resourcepool.h"
#include "commandlist.h"
#include "skybox.h"
#include "envmap.h"
#include <tuple>


// Renderables
namespace Renderable
{
  enum class Type : uint16_t
  {
    Clear,
    Sprites,
    Meshes,
    Casters,
    Lights,
    Environment,
  };

  using Vec2 = lml::Vec2;
  using Vec3 = lml::Vec3;
  using Vec4 = lml::Vec4;
  using Rect2 = lml::Rect2;
  using Color3 = lml::Color3;
  using Color4 = lml::Color4;
  using Attenuation = lml::Attenuation;
  using Transform = lml::Transform;

  struct Sprites
  {
    static const Type type = Type::Sprites;

    Rect2 viewport;

    CommandList const *commandlist;
  };

  struct Meshes
  {
    static const Type type = Type::Meshes;

    CommandList const *commandlist;
  };

  struct Casters
  {
    static const Type type = Type::Casters;

    CommandList const *commandlist;
  };

  struct Lights
  {
    static const Type type = Type::Lights;

    CommandList const *commandlist;
  };

  struct Environment
  {
    static const Type type = Type::Environment;

    Vec3 dimension;
    Transform transform;
    EnvMap const *envmap;
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

    void reset();

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

struct ShadowMap
{
  int width = 1024;
  int height = 1024;

  float shadowsplitfar = 150.0f;
  float shadowsplitlambda = 0.925f;

  static constexpr int nslices = 4;

  Vulkan::Texture shadowmap;

  std::array<float, nslices> splits;
  std::array<lml::Matrix4f, nslices> shadowview;
};

struct RenderContext
{
  RenderContext()
    : frame(0), initialised(false)
  {
  }

  Vulkan::VulkanDevice device;

  Vulkan::Fence framefence;

  Vulkan::CommandPool commandpool;
  Vulkan::CommandBuffer commandbuffers[2];

  Vulkan::TransferBuffer transferbuffer;

  Vulkan::DescriptorPool descriptorpool;

  Vulkan::QueryPool timingquerypool;

  Vulkan::DescriptorSetLayout scenesetlayout;
  Vulkan::DescriptorSetLayout materialsetlayout;
  Vulkan::DescriptorSetLayout modelsetlayout;
  Vulkan::DescriptorSetLayout computelayout;

  Vulkan::PipelineLayout pipelinelayout;

  Vulkan::PipelineCache pipelinecache;

  Vulkan::Texture colorbuffer;
  Vulkan::Texture rt0buffer;
  Vulkan::Texture rt1buffer;
  Vulkan::Texture normalbuffer;
  Vulkan::Texture depthbuffer;
  Vulkan::Texture scratchbuffers[2];
  Vulkan::FrameBuffer geometrybuffer;
  Vulkan::FrameBuffer framebuffer;

  Vulkan::RenderPass shadowpass;
  Vulkan::RenderPass geometrypass;
  Vulkan::RenderPass renderpass;

  Vulkan::ConstantBuffer constantbuffer;

  Vulkan::Texture envbrdf;

  float ssaoscale;
  lml::Vec4 ssaonoise[16];
  lml::Vec4 ssaokernel[16];
  Vulkan::Texture ssaobuffers[2];
  Vulkan::DescriptorSet ssaotargets[2];
  Vulkan::DescriptorSet ssaodescriptors[2];

  Vulkan::DescriptorSet lightingdescriptors[2];

  Vulkan::DescriptorSet skyboxdescriptors[2];
  Vulkan::CommandBuffer skyboxcommands[2];

  Vulkan::DescriptorSet ssrdescriptor;

  Vulkan::DescriptorSet bloomdescriptor;

  Vulkan::DescriptorSet scratchtargets[2];
  Vulkan::DescriptorSet colorbuffertarget;

  Vulkan::VertexAttribute vertexattributes[4];

  Vulkan::VertexBuffer unitquad;

  Vulkan::Texture whitediffuse;
  Vulkan::Texture nominalnormal;

  Vulkan::Pipeline shadowpipeline;
  Vulkan::Pipeline geometrypipeline;
  Vulkan::Pipeline ssaopipeline;
  Vulkan::Pipeline lightingpipeline;
  Vulkan::Pipeline skyboxpipeline;
  Vulkan::Pipeline ssrpipeline[2];
  Vulkan::Pipeline luminancepipeline;
  Vulkan::Pipeline bloompipeline[4];
  Vulkan::Pipeline spritepipeline;

  ShadowMap shadows;
  Vulkan::FrameBuffer shadowbuffer;

  int fbowidth, fboheight, fbox, fboy;

  ResourcePool resourcepool;

  Vulkan::MemoryView<uint8_t> transfermemory;

  size_t frame;

  bool initialised;

  Camera camera;
  lml::Matrix4f proj;
  lml::Matrix4f view;

  float luminance = 1.0;

  Camera prevcamera;
};

struct RenderParams
{
  int width = 1280;
  int height = 720;
  float aspect = 1.7777778f;

  lml::Vec3 sundirection = { -0.57735f, -0.57735f, -0.57735f };
  lml::Color3 sunintensity = { 1.0f, 1.0f, 1.0f };

  SkyBox const *skybox = nullptr;
  lml::Transform skyboxorientation = lml::Transform::identity();
  float skyboxlod = 0.0f;

  float lightfalloff = 0.66f;
  float ssaoscale = 1.0f;
  float ssrstrength = 1.0f;
  float bloomstrength = 1.0f;
};


// Prepare
bool prepare_render_context(DatumPlatform::PlatformInterface &platform, RenderContext &context, AssetManager *assets);
bool prepare_render_pipeline(RenderContext &context, RenderParams const &params);
void release_render_pipeline(RenderContext &context);

// Fallback
void render_fallback(RenderContext &context, DatumPlatform::Viewport const &viewport, void *bitmap = nullptr, int width = 0, int height = 0);

// Render
void render(RenderContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params);
