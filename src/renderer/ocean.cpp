//
// ocean.cpp
//

#include "ocean.h"
#include "datum/debug.h"
#include <random>
#include <complex>
#include <numeric>

using namespace std;
using namespace lml;
using leap::extentof;

enum ShaderLocation
{
  oceanset = 0,
  spectrum = 1,
  maptarget = 2,
  displacementmap = 3,
  vertexbuffer = 4,
};

struct OceanSet
{
  alignas(16) Matrix4f proj;
  alignas(16) Matrix4f invproj;
  alignas(16) Transform camera;

  alignas(16) Vec4 plane;
  alignas( 4) float swelllength;
  alignas( 4) float swellamplitude;
  alignas( 4) float swellsteepness;
  alignas( 4) float swellphase;
  alignas( 8) Vec2 swelldirection;

  alignas( 4) float scale;
  alignas( 4) float choppiness;
  alignas( 4) float smoothing;

  alignas( 4) uint32_t size;
  alignas( 4) float h0[OceanContext::WaveResolution * OceanContext::WaveResolution][2];
  alignas( 4) float phase[OceanContext::WaveResolution * OceanContext::WaveResolution];

  alignas( 4) uint32_t sizex;
  alignas( 4) uint32_t sizey;
};

struct Spectrum
{
  alignas(4) complex<float> h[OceanContext::WaveResolution * OceanContext::WaveResolution];
  alignas(4) complex<float> hx[OceanContext::WaveResolution * OceanContext::WaveResolution];
  alignas(4) complex<float> hy[OceanContext::WaveResolution * OceanContext::WaveResolution];

  alignas(4) float weights[OceanContext::WaveResolution * OceanContext::WaveResolution];
};

namespace
{
  float dispersion(Vec2 const &k)
  {
    float klength2 = normsqr(k);

    return sqrt(9.81f * sqrt(klength2) * (1 + klength2 / (370 * 370)));
  }

  float phillips(Vec2 const &k, float a, float v, Vec2 const &w)
  {
    if (k.x == 0 && k.y == 0)
      return 0.0f;

    float kdotw = dot(k, w);

    float d = (kdotw < 0) ? 0.2f : 1.0f;

    float L = v * v / 9.81f;
    float L2 = L * L;

    float damping = 0.001f;
    float l2 = L2 * damping * damping;

    float klength2 = normsqr(k);

    return a * d * exp(-1.0f / (klength2 * L2)) / (klength2*klength2*klength2) * (kdotw*kdotw) * exp(-klength2 * l2);
  }

  complex<float> guass_random_distribution(mt19937 &entropy)
  {
    uniform_real_distribution<float> real11{-1.0f, 1.0f};

    float x = 0, y = 0, w = 1;

    for(int i = 0; i < 8 && !(0 < w && w < 1); ++i)
    {
      x = real11(entropy);
      y = real11(entropy);
      w = x*x + y*y;
    }

    return complex<float>(x, y) * sqrt((-2*log(w)) / w);
  }
}


///////////////////////// seed_ocean ////////////////////////////////////////
void seed_ocean(OceanParams &params)
{
  int size = OceanContext::WaveResolution;

  params.swellphase = 0.0f;

  mt19937 entropy(random_device{}());

  for(int m = 0; m < size; ++m)
  {
    auto y = 2*pi<float>() * (m - 0.5f*size) / params.wavescale;

    for(int n = 0; n < size; ++n)
    {
      auto x = 2*pi<float>() * (n - 0.5f*size) / params.wavescale;

      auto s0 = guass_random_distribution(entropy);
      auto h0 = s0 * sqrt(phillips({x,y}, params.waveamplitude, params.windspeed, params.winddirection) / 2.0f);

      params.seed[m][n][0] = s0.real();
      params.seed[m][n][1] = s0.imag();
      params.height[m][n][0] = h0.real();
      params.height[m][n][1] = h0.imag();
      params.phase[m][n] = 0.0f;
    }
  }

  params.flow = Vec2(0);
}


///////////////////////// lerp_ocean_swell /////////////////////////////////
void lerp_ocean_swell(OceanParams &params, float swelllength, float swellamplitude, float swellspeed, Vec2 swelldirection, float t)
{
  params.swelllength = lerp(params.swelllength, swelllength, t);
  params.swellamplitude = lerp(params.swellamplitude, swellamplitude, t);
  params.swellspeed = lerp(params.swellspeed, swellspeed, t);
  params.swelldirection = normalise(lerp(params.swelldirection, swelldirection, t));
}


///////////////////////// lerp_ocean_waves //////////////////////////////////
void lerp_ocean_waves(OceanParams &params, float wavescale, float waveamplitude, float windspeed, Vec2 winddirection, float t)
{
  int size = OceanContext::WaveResolution;

  params.wavescale = lerp(params.wavescale, wavescale, t);
  params.waveamplitude = lerp(params.waveamplitude, waveamplitude, t);
  params.windspeed = lerp(params.windspeed, windspeed, t);
  params.winddirection = normalise(lerp(params.winddirection, winddirection, t));

  for(int m = 0; m < size; ++m)
  {
    auto y = 2*pi<float>() * (m - 0.5f*size) / params.wavescale;

    for(int n = 0; n < size; ++n)
    {
      auto x = 2*pi<float>() * (n - 0.5f*size) / params.wavescale;

      auto h0 = sqrt(phillips({x,y}, params.waveamplitude, params.windspeed, params.winddirection) / 2.0f);

      params.height[m][n][0] = params.seed[m][n][0] * h0;
      params.height[m][n][1] = params.seed[m][n][1] * h0;
    }
  }
}


///////////////////////// update_ocean ////////////////////////////////////
void update_ocean(OceanParams &params, float dt)
{
  int size = OceanContext::WaveResolution;

  params.swellphase = fmod(params.swellphase + (params.swellspeed * 2*pi<float>()/params.swelllength)*dt, 2*pi<float>());

  for(int m = 0; m < size; ++m)
  {
    auto y = 2*pi<float>() * (m - 0.5f*size) / params.wavescale;

    for(int n = 0; n < size; ++n)
    {
      auto x = 2*pi<float>() * (n - 0.5f*size) / params.wavescale;

      params.phase[m][n] = fmod(params.phase[m][n] + dispersion({x,y})*dt, 2*pi<float>());
    }
  }

  params.flow += 0.002f * params.windspeed * params.winddirection * dt;
}


///////////////////////// prepare_ocean_context /////////////////////////////
bool prepare_ocean_context(DatumPlatform::PlatformInterface &platform, OceanContext &context, AssetManager *assets, uint32_t queueindex)
{
  if (context.ready)
    return true;

  if (context.vulkan == 0)
  {
    //
    // Vulkan Device
    //

    auto renderdevice = platform.render_device();

    initialise_vulkan_device(&context.vulkan, renderdevice.physicaldevice, renderdevice.device, renderdevice.queues[queueindex].queue, renderdevice.queues[queueindex].familyindex);

    context.commandpool = create_commandpool(context.vulkan, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    context.commandbuffer = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    context.fence = create_fence(context.vulkan);

    // DescriptorPool

    VkDescriptorPoolSize typecounts[3] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    typecounts[0].descriptorCount = 3;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[1].descriptorCount = 1;
    typecounts[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    typecounts[2].descriptorCount = 2;

    VkDescriptorPoolCreateInfo descriptorpoolinfo = {};
    descriptorpoolinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorpoolinfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorpoolinfo.maxSets = accumulate(begin(typecounts), end(typecounts), 0, [](int i, auto &k) { return i + k.descriptorCount; });
    descriptorpoolinfo.poolSizeCount = extentof(typecounts);
    descriptorpoolinfo.pPoolSizes = typecounts;

    context.descriptorpool = create_descriptorpool(context.vulkan, descriptorpoolinfo);

    // PipelineCache

    VkPipelineCacheCreateInfo pipelinecacheinfo = {};
    pipelinecacheinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    context.pipelinecache = create_pipelinecache(context.vulkan, pipelinecacheinfo);

    // Transfer Buffer

    context.transferbuffer = create_transferbuffer(context.vulkan, sizeof(OceanSet));
  }

  if (context.descriptorsetlayout == 0)
  {
    // Ocean Set

    VkDescriptorSetLayoutBinding bindings[5] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[2].binding = 2;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[3].binding = 3;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[4].binding = 4;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.descriptorsetlayout = create_descriptorsetlayout(context.vulkan, createinfo);

    context.descriptorset = allocate_descriptorset(context.vulkan, context.descriptorpool, context.descriptorsetlayout);
  }

  if (context.pipelinelayout == 0)
  {
    // PipelineLayout

    VkDescriptorSetLayout layouts[1] = {};
    layouts[0] = context.descriptorsetlayout;

    VkPipelineLayoutCreateInfo pipelinelayoutinfo = {};
    pipelinelayoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelinelayoutinfo.setLayoutCount = extentof(layouts);
    pipelinelayoutinfo.pSetLayouts = layouts;

    context.pipelinelayout = create_pipelinelayout(context.vulkan, pipelinelayoutinfo);
  }

  if (context.pipeline[0] == 0)
  {
    auto cs = assets->find(CoreAsset::ocean_sim_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.pipeline[0] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.pipeline[1] == 0)
  {
    auto cs = assets->find(CoreAsset::ocean_fftx_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.pipeline[1] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.pipeline[2] == 0)
  {
    auto cs = assets->find(CoreAsset::ocean_ffty_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.pipeline[2] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.pipeline[3] == 0)
  {
    auto cs = assets->find(CoreAsset::ocean_map_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.pipeline[3] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.pipeline[4] == 0)
  {
    auto cs = assets->find(CoreAsset::ocean_gen_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets->request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pName = "main";

    context.pipeline[4] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.spectrum == 0)
  {
    Spectrum spectrum;

    for(int i = 0; i < OceanContext::WaveResolution; ++i)
    {
      for(int n = 0; n < log2(OceanContext::WaveResolution); ++n)
      {
        spectrum.weights[i * OceanContext::WaveResolution + 2*n+0] = cos(-2*pi<float>() * i / (2 * pow(2.0f, float(n))));
        spectrum.weights[i * OceanContext::WaveResolution + 2*n+1] = sin(-2*pi<float>() * i / (2 * pow(2.0f, float(n))));
      }
    }

    context.spectrum = create_storagebuffer(context.vulkan, sizeof(spectrum), &spectrum);
  }

  if (context.displacementmap == 0)
  {
    begin(context.vulkan, context.commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    context.displacementmap = create_texture(context.vulkan, context.commandbuffer, OceanContext::WaveResolution, OceanContext::WaveResolution, 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    end(context.vulkan, context.commandbuffer);

    submit(context.vulkan, context.commandbuffer, context.fence);

    wait_fence(context.vulkan, context.fence);
  }

  context.ready = true;

  return true;
}


///////////////////////// render ////////////////////////////////////////////
void render_ocean_surface(OceanContext &context, Mesh const *mesh, uint32_t sizex, uint32_t sizey, Camera const &camera, OceanParams const &params)
{
  using namespace Vulkan;

  assert(context.ready);
  assert(mesh->vertexbuffer.vertexcount == sizex*sizey);

  auto &commandbuffer = context.commandbuffer;

  auto oceanset = map_memory<OceanSet>(context.vulkan, context.transferbuffer, 0, context.transferbuffer.size);

  oceanset->proj = camera.proj();
  oceanset->invproj = inverse(oceanset->proj);
  oceanset->camera = camera.transform();

  oceanset->plane = Vec4(params.plane.normal, params.plane.distance);

  oceanset->swelllength = params.swelllength;
  oceanset->swellamplitude = params.swellamplitude;
  oceanset->swellsteepness = params.swellsteepness;
  oceanset->swelldirection = params.swelldirection;
  oceanset->swellphase = params.swellphase;

  oceanset->scale = 1 / params.wavescale;
  oceanset->choppiness = params.choppiness;
  oceanset->smoothing = 1 / params.smoothing;

  oceanset->size = OceanContext::WaveResolution;
  memcpy(oceanset->h0, params.height, sizeof(oceanset->h0));
  memcpy(oceanset->phase, params.phase, sizeof(oceanset->phase));

  oceanset->sizex = sizex;
  oceanset->sizey = sizey;

  bind_buffer(context.vulkan, context.descriptorset, ShaderLocation::oceanset, context.transferbuffer, 0, context.transferbuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  bind_buffer(context.vulkan, context.descriptorset, ShaderLocation::spectrum, context.spectrum, 0, context.spectrum.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  bind_imageview(context.vulkan, context.descriptorset, ShaderLocation::maptarget, context.displacementmap.imageview);

  bind_texture(context.vulkan, context.descriptorset, ShaderLocation::displacementmap, context.displacementmap);

  size_t verticessize = mesh->vertexbuffer.vertexcount * mesh->vertexbuffer.vertexsize;
  auto verticesbuffer = create_storagebuffer(context.vulkan, mesh->vertexbuffer.memory, mesh->vertexbuffer.verticesoffset, verticessize);

  bind_buffer(context.vulkan, context.descriptorset, ShaderLocation::vertexbuffer, verticesbuffer, 0, verticesbuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  bind_descriptor(commandbuffer, context.descriptorset, context.pipelinelayout, 0, VK_PIPELINE_BIND_POINT_COMPUTE);

  bind_pipeline(commandbuffer, context.pipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, OceanContext::WaveResolution/16, OceanContext::WaveResolution/16, 1);

  barrier(commandbuffer, context.spectrum, 0, context.spectrum.size);

  bind_pipeline(commandbuffer, context.pipeline[1], VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, 1, OceanContext::WaveResolution, 1);

  barrier(commandbuffer, context.spectrum, 0, context.spectrum.size);

  bind_pipeline(commandbuffer, context.pipeline[2], VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, OceanContext::WaveResolution, 1, 1);

  barrier(commandbuffer, context.spectrum, 0, context.spectrum.size);

  bind_pipeline(commandbuffer, context.pipeline[3], VK_PIPELINE_BIND_POINT_COMPUTE);

  setimagelayout(commandbuffer, context.displacementmap.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2 });

  dispatch(commandbuffer, OceanContext::WaveResolution/16, OceanContext::WaveResolution/16, 1);

  setimagelayout(commandbuffer, context.displacementmap.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2 });

  bind_pipeline(commandbuffer, context.pipeline[4], VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, sizex/16, sizey/16, 1);

  barrier(commandbuffer, verticesbuffer, 0, verticesbuffer.size);

  end(context.vulkan, commandbuffer);

  //
  // Submit
  //

  submit(context.vulkan, commandbuffer, context.fence);

  wait_fence(context.vulkan, context.fence);
}
