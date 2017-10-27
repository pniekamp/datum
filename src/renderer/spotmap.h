//
// Datum - SpotMap
//

//
// Copyright (c) 2017 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "texture.h"
#include "mesh.h"
#include "material.h"
#include "animation.h"
#include "commandlump.h"

struct SpotMapContext;

//|---------------------- SpotMap -------------------------------------------
//|--------------------------------------------------------------------------

class SpotMap
{

  public:
    friend SpotMap const *ResourceManager::create<SpotMap>(Asset const *asset);    
    friend SpotMap const *ResourceManager::create<SpotMap>(int width, int height);

    bool ready() const { return (state == State::Ready); }

    int width;
    int height;

    Vulkan::Texture texture;

    mutable Vulkan::FrameBuffer framebuffer;

  public:

    enum class State
    {
      Empty,
      Loading,
      Waiting,
      Testing,
      Ready,
    };

    Asset const *asset;
    ResourceManager::TransferLump const *transferlump;

    std::atomic<State> state;

  protected:
    SpotMap() = default;
};


//|---------------------- SpotCasterList ------------------------------------
//|--------------------------------------------------------------------------

class SpotCasterList
{
  public:

    VkCommandBuffer castercommands;

    operator bool() const { return m_commandlump; }

  public:

    struct BuildState
    {
      int width = 256;
      int height = 256;

      SpotMapContext *context;
      ResourceManager *resources;

      VkPipeline pipeline;

      CommandLump::Descriptor materialset;

      CommandLump::Descriptor modelset;

      CommandLump *commandlump = nullptr;

      Mesh const *mesh;
      Material const *material;
    };

    bool begin(BuildState &state, SpotMapContext &context, ResourceManager &resources);

    void push_mesh(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material);

    void push_mesh(BuildState &state, lml::Transform const &transform, Pose const &pose, Mesh const *mesh, Material const *material);

    void push_foilage(BuildState &state, lml::Transform const *transforms, size_t count, Mesh const *mesh, Material const *material, lml::Vec4 const &wind = { 0.0f, 0.0f, 0.0f, 0.0f }, lml::Vec3 const &bendscale = { 0.0f, 0.025f, 0.0f }, lml::Vec3 const &detailbendscale = { 0.0f, 0.025f, 0.0f });

    void finalise(BuildState &state);

  public:

    CommandLump const *release() { return m_commandlump.release(); }

  private:

    unique_resource<CommandLump> m_commandlump;
};


//|---------------------- SpotMapRenderer -----------------------------------
//|--------------------------------------------------------------------------

struct SpotMapContext
{
  bool ready = false;

  Vulkan::VulkanDevice vulkan;

  Vulkan::CommandPool commandpool;
  Vulkan::CommandBuffer commandbuffer;

  Vulkan::DescriptorPool descriptorpool;

  VkPipelineLayout pipelinelayout;
  VkDescriptorSetLayout materialsetlayout;
  VkDescriptorSetLayout modelsetlayout;

  Vulkan::PipelineCache pipelinecache;

  Vulkan::RenderPass renderpass;

  Vulkan::Pipeline srcblitpipeline;
  Vulkan::CommandBuffer srcblitcommands[16];
  Vulkan::DescriptorSet srcblitdescriptors[16];
  Vulkan::Sampler srcblitsampler;

  Vulkan::Pipeline modelspotmappipeline;
  Vulkan::Pipeline actorspotmappipeline;
  Vulkan::Pipeline foilagespotmappipeline;

  Vulkan::Fence fence;

  Vulkan::Semaphore rendercomplete;

  RenderContext *rendercontext;
};

struct SpotMapInfo
{
  SpotMap const *target;

  lml::Transform spotview;

  SpotMap const *source = nullptr;
  SpotCasterList const *casters = nullptr;
};

struct SpotMapParams
{
};

// Initialise
void initialise_spotmap_context(DatumPlatform::PlatformInterface &platform, SpotMapContext &context, uint32_t queueindex);

// Prepare
bool prepare_spotmap_context(DatumPlatform::PlatformInterface &platform, SpotMapContext &context, RenderContext &rendercontext, AssetManager &assets);

// Render
void render_spotmaps(SpotMapContext &context, SpotMapInfo const *spotmaps, size_t spotmapcount, SpotMapParams const &params, VkSemaphore const (&dependancies)[8] = {});
