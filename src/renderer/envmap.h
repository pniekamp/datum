//
// Datum - envmap
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "texture.h"

//|---------------------- EnvMap --------------------------------------------
//|--------------------------------------------------------------------------

class EnvMap
{
  public:

    enum class Format
    {
      RGBE,
      FLOAT16,
      FLOAT32
    };

  public:
    friend EnvMap const *ResourceManager::create<EnvMap>(Asset const *asset);
    friend EnvMap const *ResourceManager::create<EnvMap>(int width, int height, Format format);

    friend void ResourceManager::update<EnvMap>(EnvMap const *envmap, ResourceManager::TransferLump const *lump);

    bool ready() const { return (state == State::Ready); }

    int width;
    int height;
    Format format;

    Vulkan::Texture texture;

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
    EnvMap() = default;
};


//|---------------------- Convolve ------------------------------------------
//|--------------------------------------------------------------------------

struct ConvolveContext
{
  bool ready = false;

  Vulkan::VulkanDevice vulkan;

  Vulkan::CommandPool commandpool;
  Vulkan::CommandBuffer commandbuffer;

  Vulkan::DescriptorPool descriptorpool;

  Vulkan::PipelineLayout pipelinelayout;

  Vulkan::PipelineCache pipelinecache;

  Vulkan::Pipeline convolvepipeline;

  Vulkan::DescriptorSetLayout descriptorsetlayout;

  Vulkan::DescriptorSet convolvedescriptors[8];
  Vulkan::ImageView convolveimageviews[8];

  Vulkan::Fence fence;
};

struct ConvolveParams
{
  int samples = 1024;
};

// Initialise
void initialise_convolve_context(DatumPlatform::PlatformInterface &platform, ConvolveContext &context, uint32_t queueindex);

// Prepare
bool prepare_convolve_context(DatumPlatform::PlatformInterface &platform, ConvolveContext &context, AssetManager &assets);

// Convolve
void convolve(ConvolveContext &context, EnvMap const *target, ConvolveParams const &params, VkSemaphore const (&dependancies)[8] = {});


//|---------------------- Project -------------------------------------------
//|--------------------------------------------------------------------------

struct Irradiance
{
  float L[9][3];
};

struct ProjectContext
{
  bool ready = false;

  Vulkan::VulkanDevice vulkan;

  Vulkan::CommandPool commandpool;
  Vulkan::CommandBuffer commandbuffer;

  Vulkan::DescriptorPool descriptorpool;

  Vulkan::PipelineLayout pipelinelayout;

  Vulkan::PipelineCache pipelinecache;

  Vulkan::Pipeline projectpipeline;

  Vulkan::DescriptorSetLayout descriptorsetlayout;

  Vulkan::DescriptorSet projectdescriptor;

  Vulkan::StorageBuffer probebuffer;

  Vulkan::Fence fence;
};

struct ProjectParams
{
};

// Initialise
void initialise_project_context(DatumPlatform::PlatformInterface &platform, ProjectContext &context, uint32_t queueindex);

// Prepare
bool prepare_project_context(DatumPlatform::PlatformInterface &platform, ProjectContext &context, AssetManager &assets);

// Project
void project(ProjectContext &context, EnvMap const *source, Irradiance &target, ProjectParams const &params, VkSemaphore const (&dependancies)[8] = {});
