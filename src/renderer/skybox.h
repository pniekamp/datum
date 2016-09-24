//
// Datum - skybox
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "envmap.h"


//|---------------------- SkyBox --------------------------------------------
//|--------------------------------------------------------------------------

class SkyBox
{
  public:
    friend SkyBox const *ResourceManager::create<SkyBox>(Asset const *asset);
    friend SkyBox const *ResourceManager::create<SkyBox>(int width, int height, EnvMap::Format format);

    friend void ResourceManager::update<SkyBox>(SkyBox const *skybox, ResourceManager::TransferLump const *lump);

    bool ready() const { return envmap->ready(); }

    EnvMap const *envmap;

  private:
    SkyBox() = default;
};


//|---------------------- SkyBoxRenderer ------------------------------------
//|--------------------------------------------------------------------------

struct SkyboxContext
{
  SkyboxContext()
    : initialised(false)
  {
  }

  Vulkan::VulkanDevice device;

  Vulkan::Fence fence;

  Vulkan::CommandPool commandpool;
  Vulkan::CommandBuffer commandbuffer;

  Vulkan::DescriptorPool descriptorpool;

  Vulkan::PipelineLayout pipelinelayout;

  Vulkan::PipelineCache pipelinecache;

  Vulkan::Pipeline pipeline;

  Vulkan::DescriptorSetLayout descriptorsetlayout;

  Vulkan::DescriptorSet descriptorset;

  bool initialised;
};

struct SkyboxParams
{
  lml::Color3 skycolor = { 0.650f, 0.570f, 0.475f };
  lml::Color3 groundcolor = { 0.41f, 0.41f, 0.4f };
  lml::Vec3 sundirection = { -0.57735f, -0.57735f, -0.57735f };
  lml::Color3 sunintensity = { 8.0f, 7.56f, 7.88f };
  float exposure = 1.0f;
};

// Prepare
bool prepare_skybox_context(DatumPlatform::PlatformInterface &platform, SkyboxContext &context, AssetManager *assets, uint32_t queueinstance);

// Render
void render_skybox(SkyboxContext &context, SkyBox const *skybox, SkyboxParams const &params);
