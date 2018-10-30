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
#include "skybox.h"
#include "envmap.h"
#include "spotmap.h"
#include "colorlut.h"
#include "material.h"

// Renderables
namespace Renderable
{
  enum class Type : uint16_t
  {
    Clear,
    Sprites,
    Overlays,
    Geometry,
    Forward,
    Casters,
    Lights,
    Decals,
  };

  using Vec2 = lml::Vec2;
  using Vec3 = lml::Vec3;
  using Vec4 = lml::Vec4;
  using Rect2 = lml::Rect2;
  using Color3 = lml::Color3;
  using Color4 = lml::Color4;
  using Attenuation = lml::Attenuation;
  using Transform = lml::Transform;
  using Matrix4f = lml::Matrix4f;

  struct Sprites
  {
    static constexpr Type type = Type::Sprites;

    VkCommandBuffer spritecommands;
  };

  struct Overlays
  {
    static constexpr Type type = Type::Overlays;

    VkCommandBuffer overlaycommands;
  };

  struct Geometry
  {
    static constexpr Type type = Type::Geometry;

    VkCommandBuffer prepasscommands;
    VkCommandBuffer geometrycommands;
  };

  struct Forward
  {
    static constexpr Type type = Type::Forward;

    struct Command
    {
      enum class Type
      {
        bind_pipeline,
        bind_vertexbuffer,
        bind_descriptor,
        draw,
        draw_indexed

      } type;

      union
      {
        struct
        {
          Vulkan::Pipeline const *pipeline;

        } bind_pipeline;

        struct
        {
          uint32_t binding;
          Vulkan::VertexBuffer const *vertexbuffer;

        } bind_vertexbuffer;

        struct
        {
          uint32_t set;
          VkDescriptorSet descriptor;
          VkDeviceSize offset;

        } bind_descriptor;

        struct
        {
          uint32_t vertexcount;
          uint32_t instancecount;

        } draw;

        struct
        {
          uint32_t indexbase;
          uint32_t indexcount;
          uint32_t instancecount;

        } draw_indexed;
      };

      Command *next;
    };

    Command const *solidcommands;
    Command const *blendcommands;
    Command const *colorcommands;
  };

  struct Casters
  {
    static constexpr Type type = Type::Casters;

    VkCommandBuffer castercommands;
  };

  struct Lights
  {
    static constexpr Type type = Type::Lights;

    struct LightList
    {
      size_t pointlightcount;

      struct PointLight
      {
        Vec3 position;
        Color3 intensity;
        Vec4 attenuation;

      } pointlights[256];

      size_t spotlightcount;

      struct SpotLight
      {
        Vec3 position;
        Color3 intensity;
        Vec4 attenuation;
        Vec3 direction;
        float cutoff;

        Transform spotview;
        SpotMap const *spotmap;

      } spotlights[16];

      size_t probecount;

      struct Probe
      {
        Vec4 position;
        Irradiance irradiance;

      } probes[64];

      size_t environmentcount;

      struct Environment
      {
        Vec3 size;
        Transform transform;
        EnvMap const *envmap;

      } environments[8];
    };

    LightList const *lightlist;
  };

  struct Decals
  {
    static constexpr Type type = Type::Decals;

    struct DecalList
    {
      size_t decalcount;

      struct Decal
      {
        Vec3 size;
        Transform transform;
        Material const *material;
        Vec4 extent;
        float layer;
        Color4 tint;
        uint32_t mask;

      } decals[256];
    };

    DecalList const *decallist;
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

    class const_iterator
    {
      public:
      
        using value_type = Header;
        using pointer = Header const *;
        using reference = Header const &;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

      public:
        explicit const_iterator(Header const *position) : m_header(position) { }
        
        bool operator ==(const_iterator const &that) const { return m_header == that.m_header; }
        bool operator !=(const_iterator const &that) const { return m_header != that.m_header; }

        Header const &operator *() const { return *m_header; }
        Header const *operator ->() const { return &*m_header; }

        const_iterator &operator++()
        {
          m_header = reinterpret_cast<Header const *>(reinterpret_cast<char const *>(m_header) + m_header->size);

          return *this;
        }

      private:

        Header const *m_header;
    };

  public:

    using allocator_type = StackAllocator<>;

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

  return reinterpret_cast<T const *>(leap::alignto(reinterpret_cast<uintptr_t>(header + 1), alignof(T)));
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
  bool ready = false;

  Vulkan::VulkanDevice vulkan;

  Vulkan::Fence framefence;

  Vulkan::CommandPool commandpool;
  Vulkan::CommandBuffer commandbuffers[2];

  Vulkan::TransferBuffer transferbuffer;

  Vulkan::DescriptorPool descriptorpool;

  Vulkan::QueryPool timingquerypool;

  Vulkan::Sampler repeatsampler;
  Vulkan::Sampler clampedsampler;
  Vulkan::Sampler shadowsampler;

  Vulkan::DescriptorSetLayout scenesetlayout;
  Vulkan::DescriptorSetLayout materialsetlayout;
  Vulkan::DescriptorSetLayout modelsetlayout;
  Vulkan::DescriptorSetLayout extendedsetlayout;

  Vulkan::PipelineLayout pipelinelayout;

  Vulkan::PipelineCache pipelinecache;

  Vulkan::Texture colorbuffer;
  Vulkan::ImageView colormipviews[2];

  Vulkan::Texture diffusebuffer;
  Vulkan::Texture specularbuffer;
  Vulkan::Texture normalbuffer;
  Vulkan::Texture depthbuffer;
  Vulkan::Texture ssaobuffers[2];
  Vulkan::Texture scratchbuffers[4];
  Vulkan::FrameBuffer preframebuffer;
  Vulkan::FrameBuffer geometryframebuffer;
  Vulkan::FrameBuffer forwardframebuffer;

  Vulkan::Texture depthmipbuffer;
  Vulkan::ImageView depthmipviews[6];

  Vulkan::Texture esmshadowbuffer;
  Vulkan::Texture fogvolumebuffers[2];

  Vulkan::Texture rendertarget;
  Vulkan::Texture depthstencil;
  Vulkan::FrameBuffer framebuffer;

  Vulkan::RenderPass shadowpass;
  Vulkan::RenderPass prepass;
  Vulkan::RenderPass geometrypass;
  Vulkan::RenderPass forwardpass;
  Vulkan::RenderPass overlaypass;

  Vulkan::StorageBuffer sceneset;
  Vulkan::DescriptorSet scenedescriptor;

  Vulkan::DescriptorSet framedescriptors[2];

  Vulkan::VertexAttribute vertexattributes[4];

  Vulkan::VertexBuffer unitquad;

  Vulkan::Texture envbrdf;
  Vulkan::Texture whitediffuse;
  Vulkan::Texture nominalnormal;

  Vulkan::Pipeline clusterpipeline;
  Vulkan::Pipeline modelshadowpipeline;
  Vulkan::Pipeline modelprepasspipeline;
  Vulkan::Pipeline modelgeometrypipeline;
  Vulkan::Pipeline actorshadowpipeline;
  Vulkan::Pipeline actorprepasspipeline;
  Vulkan::Pipeline actorgeometrypipeline;
  Vulkan::Pipeline foilageshadowpipeline;
  Vulkan::Pipeline foilageprepasspipeline;
  Vulkan::Pipeline foilagegeometrypipeline;
  Vulkan::Pipeline terrainprepasspipeline;
  Vulkan::Pipeline terraingeometrypipeline;
  Vulkan::Pipeline depthblitpipeline;
  Vulkan::Pipeline depthmippipeline[6];
  Vulkan::Pipeline esmpipeline[3];
  Vulkan::Pipeline fogvolumepipeline[2];
  Vulkan::Pipeline lightingpipeline;
  Vulkan::Pipeline skyboxpipeline;
  Vulkan::Pipeline opaquepipeline;
  Vulkan::Pipeline translucentpipeline;
  Vulkan::Pipeline translucentblendpipeline;
  Vulkan::Pipeline ssaopipeline;
  Vulkan::Pipeline fogplanepipeline;
  Vulkan::Pipeline oceanpipeline;
  Vulkan::Pipeline waterpipeline;
  Vulkan::Pipeline particlepipeline;
  Vulkan::Pipeline particleblendpipeline;
  Vulkan::Pipeline weightblendpipeline;
  Vulkan::Pipeline ssrpipeline;
  Vulkan::Pipeline luminancepipeline;
  Vulkan::Pipeline bloompipeline[3];
  Vulkan::Pipeline colorblurpipeline[2];
  Vulkan::Pipeline compositepipeline;
  Vulkan::Pipeline spritepipeline;
  Vulkan::Pipeline gizmopipeline;
  Vulkan::Pipeline wireframepipeline;
  Vulkan::Pipeline stencilmaskpipeline;
  Vulkan::Pipeline stencilfillpipeline;
  Vulkan::Pipeline stencilpathpipeline;
  Vulkan::Pipeline linepipeline;
  Vulkan::Pipeline outlinepipeline;

  Vulkan::CommandBuffer forwardcommands[2][3];
  Vulkan::CommandBuffer compositecommands[2];

  float ssaoscale;
  Vulkan::StorageBuffer ssaoset;

  ShadowMap shadows;
  Vulkan::FrameBuffer shadowframebuffer;

  std::tuple<size_t, VkImage> decalmaps[2][16];

  int width, height;
  float scale, aspect;

  int fbowidth, fboheight, fbox, fboy;

  ResourcePool resourcepool;

  bool prepared = false;

  size_t frame;

  Camera camera;
  lml::Matrix4f proj;
  lml::Matrix4f view;

  float luminance = 1.0;

  Vulkan::Texture defer; // @ResizeHack

  Camera prevcamera;
};

enum class RenderPipelineConfig
{
  FogDepthRange = 0x07, // float
  EnableDepthOfField = 0x1A, // bool
  EnableColorGrading = 0x1B, // bool
};

struct RenderParams
{
  int width = 1280;
  int height = 720;
  float scale = 1.0f;
  float aspect = 1.7777778f;

  lml::Vec3 sundirection = { -0.57735f, -0.57735f, -0.57735f };
  lml::Color3 sunintensity = { 8.0f, 7.65f, 6.71f };
  float suncutoff = 0.995f;

  SkyBox const *skybox = nullptr;
  lml::Transform skyboxorientation = ::lml::Transform::identity();
  float skyboxlod = 0.0f;

  float ambientintensity = 1.0f;
  float specularintensity = 1.0f;

  float lightfalloff = 0.66f;
  float ssaoscale = 0.0f;
  float ssrstrength = 1.0f;
  float bloomstrength = 1.0f;

  float fogdensity = 0.1f;
  lml::Vec3 fogattenuation = { 0.0f, 0.5f, 0.0f };

  ColorLut const *colorlut = nullptr;
};

void prefetch_core_assets(DatumPlatform::PlatformInterface &platform, AssetManager &assets);

// Config
template<typename T>
void config_render_pipeline(RenderPipelineConfig config, T value);

// Initialise
void initialise_render_context(DatumPlatform::PlatformInterface &platform, RenderContext &context, size_t storagesize, uint32_t queueindex);

// Prepare
bool prepare_render_context(DatumPlatform::PlatformInterface &platform, RenderContext &context, AssetManager &assets);
void prepare_render_pipeline(RenderContext &context, RenderParams const &params);
void release_render_pipeline(RenderContext &context);

// Blit
void blit(RenderContext &context, Vulkan::Texture const &src, VkBuffer dst, VkDeviceSize offset, VkSemaphore const (&dependancies)[8] = {});
void blit(RenderContext &context, Vulkan::Texture const &src, VkImage dst, int dx, int dy, int dw, int dh, VkImageSubresourceLayers dstlayers, VkFilter filter, VkSemaphore const (&dependancies)[8] = {});

// Fallback
void render_fallback(RenderContext &context, DatumPlatform::Viewport const &viewport, const void *bitmap = nullptr, int width = 0, int height = 0);

// Render
void render(RenderContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params, VkSemaphore const (&dependancies)[7] = {});
