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
    friend SpotMap const *ResourceManager::create<SpotMap>(SpotMapContext *context, int width, int height);

    bool ready() const { return (state == State::Ready); }

    int width;
    int height;

    Vulkan::Texture texture;
    Vulkan::FrameBuffer framebuffer;

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

    void finalise(BuildState &state);

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
  VkDescriptorSetLayout scenesetlayout;
  VkDescriptorSetLayout materialsetlayout;
  VkDescriptorSetLayout modelsetlayout;

  Vulkan::RenderPass renderpass;

  Vulkan::StorageBuffer sceneset;
  Vulkan::DescriptorSet scenedescriptor;

  Vulkan::Pipeline srcblitpipeline;
  Vulkan::CommandBuffer srcblitcommands;
  Vulkan::DescriptorSet srcblitdescriptor;
  Vulkan::Sampler srcblitsampler;

  Vulkan::Pipeline modelshadowpipeline;
  Vulkan::Pipeline actorshadowpipeline;

  Vulkan::Texture whitediffuse;
  Vulkan::VertexBuffer unitquad;


  Vulkan::Fence fence;

  RenderContext *rendercontext;

  mutable leap::threadlib::SpinLock m_mutex;
};

struct SpotMapParams
{
  lml::Transform shadowview;

  SpotMap const *source = nullptr;
};

// Initialise
void initialise_spotmap_context(DatumPlatform::PlatformInterface &platform, SpotMapContext &context, uint32_t queueindex);

// Prepare
bool prepare_spotmap_context(DatumPlatform::PlatformInterface &platform, SpotMapContext &context, RenderContext &rendercontext, AssetManager &assets);

// Render
void render_spotmap(SpotMapContext &context, SpotMap const *target, SpotCasterList const &casters, SpotMapParams const &params);
