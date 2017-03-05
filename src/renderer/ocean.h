//
// ocean.h
//

#pragma once

#include "datum/renderer.h"

//|---------------------- Ocean ---------------------------------------------
//|--------------------------------------------------------------------------

struct OceanContext
{
  bool ready = false;

  static const int WaveResolution = 64;

  Vulkan::VulkanDevice vulkan;

  Vulkan::CommandPool commandpool;
  Vulkan::CommandBuffer commandbuffer;

  Vulkan::DescriptorPool descriptorpool;

  Vulkan::PipelineLayout pipelinelayout;

  Vulkan::PipelineCache pipelinecache;

  Vulkan::Pipeline pipeline[5];

  Vulkan::DescriptorSetLayout descriptorsetlayout;

  Vulkan::DescriptorSet descriptorset;

  Vulkan::StorageBuffer transferbuffer;

  Vulkan::StorageBuffer spectrum;

  Vulkan::Texture displacementmap;

  Vulkan::Fence fence;
};

struct OceanParams
{
  lml::Plane plane = { { 0.0f, 0.0f, 1.0f }, 0.0f };

  // Swell
  float swelllength = 40.0f;
  float swellamplitude = 0.8f;
  float swellsteepness = 0.0f;
  float swellspeed = 1.25f;
  lml::Vec2 swelldirection = { 0.780869f, 0.624695f };

  // Waves
  float wavescale = 64;
  float waveamplitude = 0.00002f;
  float windspeed = 30.0f;
  lml::Vec2 winddirection = { 0.780869f, 0.624695f };
  float choppiness = 1.0f;
  float smoothing = 350.0f;

  // State
  float swellphase;
  float seed[OceanContext::WaveResolution][OceanContext::WaveResolution][2];
  float height[OceanContext::WaveResolution][OceanContext::WaveResolution][2];
  float phase[OceanContext::WaveResolution][OceanContext::WaveResolution];
};

void seed_ocean(OceanParams &params);
void lerp_ocean_swell(OceanParams &params, float swelllength, float swellamplitude, float swellspeed, lml::Vec2 swelldirection, float t);
void lerp_ocean_waves(OceanParams &params, float wavescale, float waveamplitude, float windspeed, lml::Vec2 winddirection, float t);
void update_ocean(OceanParams &params, float dt);

// Prepare
bool prepare_ocean_context(DatumPlatform::PlatformInterface &platform, OceanContext &context, AssetManager *assets, uint32_t queueindex);

// Render
void render_ocean_surface(OceanContext &context, Mesh const *mesh, uint32_t sizex, uint32_t sizey, Camera const &camera, OceanParams const &params);
