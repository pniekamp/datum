//
// Datum - renderer
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "renderer.h"
#include "corepack.h"
#include "assetpack.h"
#include <leap.h>
#include <leap/lml/matrixconstants.h>
#include <vector>
#include <limits>
#include <random>
#include <numeric>
#include "debug.h"

using namespace std;
using namespace lml;
using namespace Vulkan;
using leap::alignto;
using leap::extentof;

enum VertexLayout
{
  position_offset = 0,
  texcoord_offset = 12,
  normal_offset = 20,
  tangent_offset = 32,

  stride = 48
};

enum ShaderLocation
{
  // Bindings

  vertex_position = 0,
  vertex_texcoord = 1,
  vertex_normal = 2,
  vertex_tangent = 3,

  rig_bone = 4,
  rig_weight = 5,

  sceneset = 0,
  materialset = 1,
  modelset = 2,
  extendedset = 3,

  scenebuf = 0,
  colormap = 1,
  diffusemap = 2,
  specularmap = 3,
  normalmap = 4,
  depthmap = 5,
  depthmipmap = 6,

  colortarget = 7,
  colormiptarget = 8,
  depthmipsrc = 9,
  depthmiptarget = 10,
  depthattachment = 11,
  ssaomap = 12,
  shadowmap = 13,
  envbrdf = 14,
  envmaps = 15,
  spotmaps = 16,
  decalmaps = 17,
  fogmap = 18,
  esmmap = 19,
  esmtarget = 20,
  ssaobuf = 21,
  ssaoprevmap = 22,
  ssaotarget = 23,
  fogdensitymap = 24,
  fogdensitytarget = 25,
  fogscattertarget = 26,
  scratchmap0 = 27,
  scratchmap1 = 28,
  scratchmap2 = 29,
  scratchmap3 = 30,
  scratchtarget0 = 31,
  scratchtarget1 = 32,
  scratchtarget2 = 33,
  scratchtarget3 = 34,
  colorlut = 35,
  lumabuf = 36,

  // Constant Ids

  SizeX = 1,
  SizeY = 2,
  SizeZ = 3,

  ClusterTileX = 4,
  ClusterTileY = 5,

  ShadowSlices = 6,

  FogDepthRange = 7,
  FogDepthExponent = 8,

  NoiseSize = 62,
  KernelSize = 63,

  SSAORadius = 67,

  BloomCutoff = 81,
  BloomBlurRadius = 84,
  BloomBlurSigma = 85,

  ColorBlurRadius = 84,
  ColorBlurSigma = 85,

  DepthOfField = 26,
  ColorGrading = 27,

  DepthMipLayer = 23,

  ESMBlurRadius = 84,

  FogVolumeX = 16,
  FogVolumeY = 17,
  FogVolumeZ = 18,

  CutOut = 31,

  SoftParticles = 28,

  DecalMask = 52,
};

struct MainLight
{
  alignas(16) Vec3 direction;
  alignas(16) Color3 intensity;

  alignas( 4) float splits[4];
  alignas(16) Matrix4f shadowview[4];
};

struct PointLight
{
  alignas(16) Vec3 position;
  alignas(16) Color3 intensity;
  alignas(16) Vec4 attenuation;
};

struct SpotLight
{
  alignas(16) Vec3 position;
  alignas(16) Color3 intensity;
  alignas(16) Vec4 attenuation;
  alignas(16) Vec3 direction;
  alignas( 4) float cutoff;
  alignas(16) Transform shadowview;
};

struct Probe
{
  alignas(16) Vec4 position;
  alignas( 4) Irradiance irradiance;
};

struct Environment
{
  alignas(16) Vec3 halfdim;
  alignas(16) Transform invtransform;
};

struct Decal
{
  alignas(16) Vec3 halfdim;
  alignas(16) Transform invtransform;
  alignas(16) Color4 color;
  alignas( 4) float metalness;
  alignas( 4) float roughness;
  alignas( 4) float reflectivity;
  alignas( 4) float emissive;
  alignas(16) Vec4 texcoords;
  alignas( 4) float layer;
  alignas( 4) uint32_t albedomap;
  alignas( 4) uint32_t normalmap;
  alignas( 4) uint32_t mask;
};

struct CameraView
{
  alignas(16) Vec3 position;
  alignas( 4) float exposure;
  alignas( 4) float focalwidth;
  alignas( 4) float focaldistance;
  alignas( 4) float skyboxlod;
  alignas( 4) float ambientintensity;
  alignas( 4) float specularintensity;
  alignas( 4) float ssrstrength;
  alignas( 4) float bloomstrength;
  alignas(16) Vec4 fogdensity;
  alignas( 4) uint32_t frame;
};

struct SceneSet
{
  alignas(16) Matrix4f proj;
  alignas(16) Matrix4f invproj;
  alignas(16) Matrix4f view;
  alignas(16) Matrix4f invview;
  alignas(16) Matrix4f worldview;
  alignas(16) Matrix4f prevview;
  alignas(16) Matrix4f skyview;
  alignas(16) Vec4 viewport;

  alignas(16) CameraView camera;

  alignas(16) MainLight mainlight;

  alignas( 4) uint32_t environmentcount;
  alignas(16) Environment environments[8];

  alignas( 4) uint32_t pointlightcount;
  alignas(16) PointLight pointlights[512];

  alignas( 4) uint32_t spotlightcount;
  alignas(16) SpotLight spotlights[16];

  alignas( 4) uint32_t probecount;
  alignas(16) Probe probes[128];

  alignas( 4) uint32_t decalcount;
  alignas(16) Decal decals[128];

  // Cluster cluster[];
};

struct SSAOSet
{
  alignas(16) Vec4 noise[16];
  alignas(16) Vec4 kernel[16];
};

struct LumaSet
{
  float luminance;
};

static constexpr size_t SceneBufferSize = 4*1024*1024;
static constexpr size_t TransferBufferSize = 512*1024;
static constexpr size_t PushConstantBufferSize = 64;

static struct ComputeConstants
{
  uint32_t ShadowSlices = ShadowMap::nslices;

  uint32_t ClusterTileX = 64;
  uint32_t ClusterTileY = 64;

  uint32_t ClusterDispatch[3] = { ClusterTileX, ClusterTileY, 1 };

  uint32_t DepthMipDispatch[3] = { 16, 16, 1 };
  uint32_t DepthMipSizeX = DepthMipDispatch[0];
  uint32_t DepthMipSizeY = DepthMipDispatch[1];

  uint32_t ESMGenDispatch[3] = { 16, 16, 1 };
  uint32_t ESMGenSizeX = ESMGenDispatch[0];
  uint32_t ESMGenSizeY = ESMGenDispatch[1];

  uint32_t ESMBlurRadius = 2;
  uint32_t ESMHBlurDispatch[3] = { 64, 1, 1 };
  uint32_t ESMHBlurSize = ESMHBlurDispatch[0] + ESMBlurRadius + ESMBlurRadius;
  uint32_t ESMVBlurDispatch[3] = { 1, 64, 1 };
  uint32_t ESMVBlurSize = ESMVBlurDispatch[1] + ESMBlurRadius + ESMBlurRadius;

  uint32_t NoiseSize = extent<decltype(SSAOSet::noise)>::value;
  uint32_t KernelSize = extent<decltype(SSAOSet::kernel)>::value;

  uint32_t SSAORadius = 2;
  uint32_t SSAODispatch[3] = { 28, 28, 1 };
  uint32_t SSAOSizeX = SSAODispatch[0] + SSAORadius + SSAORadius;
  uint32_t SSAOSizeY = SSAODispatch[1] + SSAORadius + SSAORadius;

  uint32_t FogVolumeX = 160;
  uint32_t FogVolumeY = 90;
  uint32_t FogVolumeZ = 64;

  float FogDepthRange = 50.0f;
  float FogDepthExponent = 3.0f;

  uint32_t FogDensityDispatch[3] = { 8, 4, 4 };
  uint32_t FogDensitySizeX = FogDensityDispatch[0];
  uint32_t FogDensitySizeY = FogDensityDispatch[1];
  uint32_t FogDensitySizeZ = FogDensityDispatch[2];

  uint32_t FogScatterDispatch[3] = { 8, 8, 1 };
  uint32_t FogScatterSizeX = FogScatterDispatch[0];
  uint32_t FogScatterSizeY = FogScatterDispatch[1];
  uint32_t FogScatterSizeZ = FogScatterDispatch[2];

  uint32_t LightingDispatch[3] = { 16, 16, 1 };
  uint32_t LightingSizeX = LightingDispatch[0];
  uint32_t LightingSizeY = LightingDispatch[1];

  uint32_t SSRDispatch[3] = { 4, 32, 1 };
  uint32_t SSRSizeX = SSRDispatch[0];
  uint32_t SSRSizeY = SSRDispatch[1];

  float BloomCutoff = 7.8f;
  uint32_t BloomLumaDispatch[3] = { 4, 32, 1 };
  uint32_t BloomLumaSizeX = BloomLumaDispatch[0];
  uint32_t BloomLumaSizeY = BloomLumaDispatch[1];

  uint32_t BloomBlurSigma = 8;
  uint32_t BloomBlurRadius = 16;
  uint32_t BloomHBlurDispatch[3] = { 64, 1, 1 };
  uint32_t BloomHBlurSize = BloomHBlurDispatch[0] + BloomBlurRadius + BloomBlurRadius;
  uint32_t BloomVBlurDispatch[3] = { 1, 64, 1 };
  uint32_t BloomVBlurSize = BloomVBlurDispatch[1] + BloomBlurRadius + BloomBlurRadius;

  uint32_t ColorBlurSigma = 4;
  uint32_t ColorBlurRadius = 16;
  uint32_t ColorHBlurDispatch[3] = { 64, 1, 1 };
  uint32_t ColorHBlurSize = ColorHBlurDispatch[0] + ColorBlurRadius + ColorBlurRadius;
  uint32_t ColorVBlurDispatch[3] = { 1, 64, 1 };
  uint32_t ColorVBlurSize = ColorVBlurDispatch[1] + ColorBlurRadius + ColorBlurRadius;

  uint32_t DepthOfField = false;
  uint32_t ColorGrading = false;

  uint32_t True = true;
  uint32_t False = false;

  uint32_t One = 1;
  uint32_t Two = 2;
  uint32_t Three = 3;
  uint32_t Four = 4;

  uint32_t Layers[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

} computeconstants;


//|---------------------- PushBuffer ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// PushBuffer::Constructor ///////////////////////////
PushBuffer::PushBuffer(allocator_type const &allocator, size_t slabsize)
{
  m_slabsize = slabsize;
  m_slab = allocate<char>(allocator, m_slabsize, alignof(Header));

  m_tail = m_slab;
}


///////////////////////// PushBuffer::reset /////////////////////////////////
void PushBuffer::reset()
{
  m_tail = m_slab;
}


///////////////////////// PushBuffer::push //////////////////////////////////
void *PushBuffer::push(Renderable::Type type, size_t size, size_t alignment)
{
  assert(((size_t)m_tail & (alignof(Header)-1)) == 0);
  assert(size + alignment + sizeof(Header) + alignof(Header) < std::numeric_limits<decltype(Header::size)>::max());

  auto header = reinterpret_cast<Header*>(m_tail);

  void *aligned = header + 1;

  size_t space = m_slabsize - (reinterpret_cast<size_t>(aligned) - reinterpret_cast<size_t>(m_slab));

  size = ((size - 1)/alignof(Header) + 1) * alignof(Header);

  if (!std::align(alignment, size, aligned, space))
    return nullptr;

  header->type = type;
  header->size = reinterpret_cast<size_t>(aligned) + size - reinterpret_cast<size_t>(header);

  m_tail = static_cast<char*>(aligned) + size;

  return aligned;
}


//|---------------------- Renderer ------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// prefetch_core_assets //////////////////////////////
void prefetch_core_assets(DatumPlatform::PlatformInterface &platform, AssetManager &assets)
{
  for(int i = 1; i < CoreAsset::core_asset_count; ++i)
  {
    assets.request(platform, assets.find(i));
  }
}


///////////////////////// config_render_pipeline ////////////////////////////
template<>
void config_render_pipeline<bool>(RenderPipelineConfig config, bool value)
{
  switch(config)
  {
    case RenderPipelineConfig::EnableDepthOfField:
      computeconstants.DepthOfField = true;
      break;

    case RenderPipelineConfig::EnableColorGrading:
      computeconstants.ColorGrading = true;
      break;

    default:
      throw std::logic_error("Unknown PipelineConfig bool value");
  }
}

template<>
void config_render_pipeline<float>(RenderPipelineConfig config, float value)
{
  switch(config)
  {
    case RenderPipelineConfig::FogDepthRange:
      computeconstants.FogDepthRange = value;
      break;

    default:
      throw std::logic_error("Unknown PipelineConfig float value");
  }
}


///////////////////////// initialise_render_context //////////////////////////
void initialise_render_context(DatumPlatform::PlatformInterface &platform, RenderContext &context, size_t storagesize, uint32_t queueindex)
{
  //
  // Vulkan Device
  //

  auto renderdevice = platform.render_device();

  initialise_vulkan_device(&context.vulkan, renderdevice.physicaldevice, renderdevice.device, renderdevice.queues[queueindex].queue, renderdevice.queues[queueindex].familyindex);

  // Command Buffers

  context.commandpool = create_commandpool(context.vulkan, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  context.commandbuffers[0] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  context.commandbuffers[1] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  // Transfer Buffer

  context.transferbuffer = create_transferbuffer(context.vulkan, TransferBufferSize);

  context.framefence = create_fence(context.vulkan, VK_FENCE_CREATE_SIGNALED_BIT);

  // Resource Pool

  initialise_resource_pool(platform, context.resourcepool, storagesize, queueindex);

  context.frame = 1;
}


///////////////////////// prepare_render_context ////////////////////////////
bool prepare_render_context(DatumPlatform::PlatformInterface &platform, RenderContext &context, AssetManager &assets)
{
  if (context.ready)
    return true;

  assert(context.vulkan);

  if (context.descriptorpool == 0)
  {
    // DescriptorPool

    VkDescriptorPoolSize typecounts[4] = {};
    typecounts[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    typecounts[0].descriptorCount = 9;
    typecounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    typecounts[1].descriptorCount = 192;
    typecounts[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    typecounts[2].descriptorCount = 48;
    typecounts[3].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    typecounts[3].descriptorCount = 3;

    VkDescriptorPoolCreateInfo descriptorpoolinfo = {};
    descriptorpoolinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorpoolinfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorpoolinfo.maxSets = accumulate(begin(typecounts), end(typecounts), 0, [](int i, auto &k) { return i + k.descriptorCount; });
    descriptorpoolinfo.poolSizeCount = extentof(typecounts);
    descriptorpoolinfo.pPoolSizes = typecounts;

    context.descriptorpool = create_descriptorpool(context.vulkan, descriptorpoolinfo);
  }

  if (context.pipelinecache == 0)
  {
    // PipelineCache

    VkPipelineCacheCreateInfo pipelinecacheinfo = {};
    pipelinecacheinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    context.pipelinecache = create_pipelinecache(context.vulkan, pipelinecacheinfo);
  }

  if (context.timingquerypool == 0)
  {
    // Timing QueryPool

    VkQueryPoolCreateInfo querypoolinfo = {};
    querypoolinfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    querypoolinfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    querypoolinfo.queryCount = 16;

    context.timingquerypool = create_querypool(context.vulkan, querypoolinfo);
  }

  if (context.pipelinelayout == 0)
  {
    // Vertex Attribute Array

    context.vertexattributes[0] = {};
    context.vertexattributes[0].location = ShaderLocation::vertex_position;
    context.vertexattributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    context.vertexattributes[0].offset = VertexLayout::position_offset;

    context.vertexattributes[1] = {};
    context.vertexattributes[1].location = ShaderLocation::vertex_texcoord;
    context.vertexattributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    context.vertexattributes[1].offset = VertexLayout::texcoord_offset;

    context.vertexattributes[2] = {};
    context.vertexattributes[2].location = ShaderLocation::vertex_normal;
    context.vertexattributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    context.vertexattributes[2].offset = VertexLayout::normal_offset;

    context.vertexattributes[3] = {};
    context.vertexattributes[3].location = ShaderLocation::vertex_tangent;
    context.vertexattributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    context.vertexattributes[3].offset = VertexLayout::tangent_offset;
  }

  if (context.scenesetlayout == 0)
  {
    // Scene Set

    VkDescriptorSetLayoutBinding bindings[37] = {};
    bindings[0].binding = ShaderLocation::scenebuf;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = ShaderLocation::colormap;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[2].binding = ShaderLocation::diffusemap;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[3].binding = ShaderLocation::specularmap;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[4].binding = ShaderLocation::normalmap;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[5].binding = ShaderLocation::depthmap;
    bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].descriptorCount = 1;
    bindings[6].binding = ShaderLocation::depthmipmap;
    bindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[6].descriptorCount = 1;
    bindings[7].binding = ShaderLocation::colortarget;
    bindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[7].descriptorCount = 1;
    bindings[8].binding = ShaderLocation::colormiptarget;
    bindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[8].descriptorCount = 1;
    bindings[9].binding = ShaderLocation::depthmipsrc;
    bindings[9].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[9].descriptorCount = 1;
    bindings[10].binding = ShaderLocation::depthmiptarget;
    bindings[10].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[10].descriptorCount = 6;
    bindings[11].binding = ShaderLocation::depthattachment;
    bindings[11].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    bindings[11].descriptorCount = 1;
    bindings[12].binding = ShaderLocation::ssaomap;
    bindings[12].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[12].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[12].descriptorCount = 1;
    bindings[13].binding = ShaderLocation::shadowmap;
    bindings[13].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[13].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[13].descriptorCount = 1;
    bindings[14].binding = ShaderLocation::envbrdf;
    bindings[14].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[14].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[14].descriptorCount = 1;
    bindings[15].binding = ShaderLocation::envmaps;
    bindings[15].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[15].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[15].descriptorCount = extent<decltype(SceneSet::environments)>::value;
    bindings[16].binding = ShaderLocation::spotmaps;
    bindings[16].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[16].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[16].descriptorCount = extent<decltype(SceneSet::spotlights)>::value;
    bindings[17].binding = ShaderLocation::decalmaps;
    bindings[17].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[17].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[17].descriptorCount = 16;
    bindings[18].binding = ShaderLocation::fogmap;
    bindings[18].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[18].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[18].descriptorCount = 1;
    bindings[19].binding = ShaderLocation::esmmap;
    bindings[19].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[19].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[19].descriptorCount = 1;
    bindings[20].binding = ShaderLocation::esmtarget;
    bindings[20].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[20].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[20].descriptorCount = 1;
    bindings[21].binding = ShaderLocation::ssaobuf;
    bindings[21].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[21].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[21].descriptorCount = 1;
    bindings[22].binding = ShaderLocation::ssaoprevmap;
    bindings[22].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[22].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[22].descriptorCount = 1;
    bindings[23].binding = ShaderLocation::ssaotarget;
    bindings[23].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[23].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[23].descriptorCount = 1;
    bindings[24].binding = ShaderLocation::fogdensitymap;
    bindings[24].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[24].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[24].descriptorCount = 1;
    bindings[25].binding = ShaderLocation::fogdensitytarget;
    bindings[25].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[25].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[25].descriptorCount = 1;
    bindings[26].binding = ShaderLocation::fogscattertarget;
    bindings[26].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[26].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[26].descriptorCount = 1;
    bindings[27].binding = ShaderLocation::scratchmap0;
    bindings[27].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[27].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[27].descriptorCount = 1;
    bindings[28].binding = ShaderLocation::scratchmap1;
    bindings[28].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[28].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[28].descriptorCount = 1;
    bindings[29].binding = ShaderLocation::scratchmap2;
    bindings[29].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[29].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[29].descriptorCount = 1;
    bindings[30].binding = ShaderLocation::scratchmap3;
    bindings[30].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[30].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[30].descriptorCount = 1;
    bindings[31].binding = ShaderLocation::scratchtarget0;
    bindings[31].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[31].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[31].descriptorCount = 1;
    bindings[32].binding = ShaderLocation::scratchtarget1;
    bindings[32].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[32].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[32].descriptorCount = 1;
    bindings[33].binding = ShaderLocation::scratchtarget2;
    bindings[33].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[33].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[33].descriptorCount = 1;
    bindings[34].binding = ShaderLocation::scratchtarget3;
    bindings[34].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[34].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[34].descriptorCount = 1;
    bindings[35].binding = ShaderLocation::colorlut;
    bindings[35].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[35].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[35].descriptorCount = 1;
    bindings[36].binding = ShaderLocation::lumabuf;
    bindings[36].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[36].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[36].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.scenesetlayout = create_descriptorsetlayout(context.vulkan, createinfo);
  }

  if (context.materialsetlayout == 0)
  {
    // Material Set

    VkDescriptorSetLayoutBinding bindings[4] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[2].binding = 2;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[3].binding = 3;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.materialsetlayout = create_descriptorsetlayout(context.vulkan, createinfo);
  }

  if (context.modelsetlayout == 0)
  {
    // Model Set

    VkDescriptorSetLayoutBinding bindings[1] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    bindings[0].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.modelsetlayout = create_descriptorsetlayout(context.vulkan, createinfo);
  }

  if (context.extendedsetlayout == 0)
  {
    // Extended Set

    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[1].binding = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createinfo.bindingCount = extentof(bindings);
    createinfo.pBindings = bindings;

    context.extendedsetlayout = create_descriptorsetlayout(context.vulkan, createinfo);
  }

  if (context.pipelinelayout == 0)
  {
    // PipelineLayout

    VkPushConstantRange constants[1] = {};
    constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    constants[0].offset = 0;
    constants[0].size = PushConstantBufferSize;

    VkDescriptorSetLayout layouts[4] = {};
    layouts[0] = context.scenesetlayout;
    layouts[1] = context.materialsetlayout;
    layouts[2] = context.modelsetlayout;
    layouts[3] = context.extendedsetlayout;

    VkPipelineLayoutCreateInfo pipelinelayoutinfo = {};
    pipelinelayoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelinelayoutinfo.pushConstantRangeCount = extentof(constants);
    pipelinelayoutinfo.pPushConstantRanges = constants;
    pipelinelayoutinfo.setLayoutCount = extentof(layouts);
    pipelinelayoutinfo.pSetLayouts = layouts;

    context.pipelinelayout = create_pipelinelayout(context.vulkan, pipelinelayoutinfo);
  }

  if (context.shadowpass == 0)
  {
    //
    // Shadow Pass
    //

    VkAttachmentDescription attachments[1] = {};
    attachments[0].format = VK_FORMAT_D32_SFLOAT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthreference = {};
    depthreference.attachment = 0;
    depthreference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[1] = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].pDepthStencilAttachment = &depthreference;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;

    context.shadowpass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.prepass == 0)
  {
    //
    // Pre Pass
    //

    VkAttachmentDescription attachments[1] = {};
    attachments[0].format = VK_FORMAT_D32_SFLOAT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthreference = {};
    depthreference.attachment = 0;
    depthreference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[1] = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].pDepthStencilAttachment = &depthreference;

    VkSubpassDependency dependencies[1] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;
    renderpassinfo.dependencyCount = extentof(dependencies);
    renderpassinfo.pDependencies = dependencies;

    context.prepass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.geometrypass == 0)
  {
    //
    // Geometry Pass
    //

    VkAttachmentDescription attachments[4] = {};
    attachments[0].format = VK_FORMAT_B8G8R8A8_SRGB;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].format = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].format = VK_FORMAT_D32_SFLOAT;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorreference[3] = {};
    colorreference[0].attachment = 0;
    colorreference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorreference[1].attachment = 1;
    colorreference[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorreference[2].attachment = 2;
    colorreference[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthreference = {};
    depthreference.attachment = 3;
    depthreference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[1] = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = extentof(colorreference);
    subpasses[0].pColorAttachments = colorreference;
    subpasses[0].pDepthStencilAttachment = &depthreference;

    VkSubpassDependency dependencies[1] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;
    renderpassinfo.dependencyCount = extentof(dependencies);
    renderpassinfo.pDependencies = dependencies;

    context.geometrypass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.forwardpass == 0)
  {
    //
    // Forward Pass
    //

    VkAttachmentDescription attachments[6] = {};
    attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].format = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[4].format = VK_FORMAT_R32_SFLOAT;
    attachments[4].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[4].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[4].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[5].format = VK_FORMAT_D32_SFLOAT;
    attachments[5].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[5].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[5].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[5].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[5].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorreference[5] = {};
    colorreference[0].attachment = 0;
    colorreference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorreference[1].attachment = 1;
    colorreference[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorreference[2].attachment = 2;
    colorreference[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorreference[3].attachment = 3;
    colorreference[3].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorreference[4].attachment = 4;
    colorreference[4].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthreference = {};
    depthreference.attachment = 5;
    depthreference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference inputinputattachments[1] = {};
    inputinputattachments[0].attachment = 5;
    inputinputattachments[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkSubpassDescription subpasses[3] = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = 3;
    subpasses[0].pColorAttachments = &colorreference[0];
    subpasses[0].pDepthStencilAttachment = &depthreference;
    subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[1].colorAttachmentCount = 2;
    subpasses[1].pColorAttachments = &colorreference[3];
    subpasses[1].inputAttachmentCount = 1;
    subpasses[1].pInputAttachments = &inputinputattachments[0];
    subpasses[1].pDepthStencilAttachment = &inputinputattachments[0];
    subpasses[2].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[2].colorAttachmentCount = 1;
    subpasses[2].pColorAttachments = &colorreference[0];
    subpasses[2].inputAttachmentCount = 1;
    subpasses[2].pInputAttachments = &inputinputattachments[0];
    subpasses[2].pDepthStencilAttachment = &inputinputattachments[0];

    VkSubpassDependency dependencies[3] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[2].srcSubpass = 1;
    dependencies[2].dstSubpass = 2;
    dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;
    renderpassinfo.dependencyCount = extentof(dependencies);
    renderpassinfo.pDependencies = dependencies;

    context.forwardpass = create_renderpass(context.vulkan, renderpassinfo);

    for(size_t k = 0; k < extent<decltype(context.forwardcommands), 0>::value; ++k)
    {
      for(size_t i = 0; i < extent<decltype(context.forwardcommands), 1>::value; ++i)
      {
        context.forwardcommands[k][i] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
      }
    }
  }

  if (context.overlaypass == 0)
  {
    //
    // Overlay Pass
    //

    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = VK_FORMAT_B8G8R8A8_SRGB;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    attachments[1].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorreference[1] = {};
    colorreference[0].attachment = 0;
    colorreference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthreference = {};
    depthreference.attachment = 1;
    depthreference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpasses[1] = {};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = extentof(colorreference);
    subpasses[0].pColorAttachments = colorreference;
    subpasses[0].pDepthStencilAttachment = &depthreference;

    VkSubpassDependency dependencies[1] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderpassinfo = {};
    renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassinfo.attachmentCount = extentof(attachments);
    renderpassinfo.pAttachments = attachments;
    renderpassinfo.subpassCount = extentof(subpasses);
    renderpassinfo.pSubpasses = subpasses;
    renderpassinfo.dependencyCount = extentof(dependencies);
    renderpassinfo.pDependencies = dependencies;

    context.overlaypass = create_renderpass(context.vulkan, renderpassinfo);
  }

  if (context.sceneset == 0)
  {
    context.sceneset = create_storagebuffer(context.vulkan, SceneBufferSize);

    context.scenedescriptor = allocate_descriptorset(context.vulkan, context.descriptorpool, context.scenesetlayout);
  }

  if (context.clusterpipeline == 0)
  {
    //
    // Cluster Pipeline
    //

    auto cs = assets.find(CoreAsset::cluster_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.clusterpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.modelshadowpipeline == 0)
  {
    //
    // Model Shadow Pipeline
    //

    auto vs = assets.find(CoreAsset::model_shadow_vert);
    auto gs = assets.find(CoreAsset::shadow_geom);
    auto fs = assets.find(CoreAsset::shadow_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pSpecializationInfo = &specializationinfo;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.shadowpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.modelshadowpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.modelprepasspipeline == 0)
  {
    //
    // Model Prepass Pipeline
    //

    auto vs = assets.find(CoreAsset::model_prepass_vert);
    auto fs = assets.find(CoreAsset::prepass_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.prepass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.modelprepasspipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.modelgeometrypipeline == 0)
  {
    //
    // Model Pipeline
    //

    auto vs = assets.find(CoreAsset::model_geometry_vert);
    auto fs = assets.find(CoreAsset::geometry_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[3] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_EQUAL;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[5] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };
    specializationmap[4] = { ShaderLocation::DecalMask, offsetof(ComputeConstants, Two), sizeof(ComputeConstants::Two) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.geometrypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.modelgeometrypipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.actorshadowpipeline == 0)
  {
    //
    // Actor Shadow Pipeline
    //

    auto vs = assets.find(CoreAsset::actor_shadow_vert);
    auto gs = assets.find(CoreAsset::shadow_geom);
    auto fs = assets.find(CoreAsset::shadow_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[2] = {};
    vertexbindings[0].binding = 0;
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexbindings[1].binding = 1;
    vertexbindings[1].stride = 32;
    vertexbindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexattributes[6] = {};
    vertexattributes[0] = context.vertexattributes[0];
    vertexattributes[1] = context.vertexattributes[1];
    vertexattributes[2] = context.vertexattributes[2];
    vertexattributes[3] = context.vertexattributes[3];
    vertexattributes[4].binding = 1;
    vertexattributes[4].location = ShaderLocation::rig_bone;
    vertexattributes[4].format = VK_FORMAT_R32G32B32A32_UINT;
    vertexattributes[4].offset = 0;
    vertexattributes[5].binding = 1;
    vertexattributes[5].location = ShaderLocation::rig_weight;
    vertexattributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertexattributes[5].offset = 16;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(vertexattributes);
    vertexinput.pVertexAttributeDescriptions = vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pSpecializationInfo = &specializationinfo;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.shadowpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.actorshadowpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.actorprepasspipeline == 0)
  {
    //
    // Actor Prepass Pipeline
    //

    auto vs = assets.find(CoreAsset::actor_prepass_vert);
    auto fs = assets.find(CoreAsset::prepass_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[2] = {};
    vertexbindings[0].binding = 0;
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexbindings[1].binding = 1;
    vertexbindings[1].stride = 32;
    vertexbindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexattributes[6] = {};
    vertexattributes[0] = context.vertexattributes[0];
    vertexattributes[1] = context.vertexattributes[1];
    vertexattributes[2] = context.vertexattributes[2];
    vertexattributes[3] = context.vertexattributes[3];
    vertexattributes[4].binding = 1;
    vertexattributes[4].location = ShaderLocation::rig_bone;
    vertexattributes[4].format = VK_FORMAT_R32G32B32A32_UINT;
    vertexattributes[4].offset = 0;
    vertexattributes[5].binding = 1;
    vertexattributes[5].location = ShaderLocation::rig_weight;
    vertexattributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertexattributes[5].offset = 16;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(vertexattributes);
    vertexinput.pVertexAttributeDescriptions = vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.prepass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.actorprepasspipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.actorgeometrypipeline == 0)
  {
    //
    // Actor Pipeline
    //

    auto vs = assets.find(CoreAsset::actor_geometry_vert);
    auto fs = assets.find(CoreAsset::geometry_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[2] = {};
    vertexbindings[0].binding = 0;
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexbindings[1].binding = 1;
    vertexbindings[1].stride = 32;
    vertexbindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexattributes[6] = {};
    vertexattributes[0] = context.vertexattributes[0];
    vertexattributes[1] = context.vertexattributes[1];
    vertexattributes[2] = context.vertexattributes[2];
    vertexattributes[3] = context.vertexattributes[3];
    vertexattributes[4].binding = 1;
    vertexattributes[4].location = ShaderLocation::rig_bone;
    vertexattributes[4].format = VK_FORMAT_R32G32B32A32_UINT;
    vertexattributes[4].offset = 0;
    vertexattributes[5].binding = 1;
    vertexattributes[5].location = ShaderLocation::rig_weight;
    vertexattributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertexattributes[5].offset = 16;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(vertexattributes);
    vertexinput.pVertexAttributeDescriptions = vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[3] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_EQUAL;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.geometrypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.actorgeometrypipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.foilageshadowpipeline == 0)
  {
    //
    // Foilage Shadow Pipeline
    //

    auto vs = assets.find(CoreAsset::foilage_shadow_vert);
    auto gs = assets.find(CoreAsset::shadow_geom);
    auto fs = assets.find(CoreAsset::shadow_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pSpecializationInfo = &specializationinfo;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.shadowpass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.foilageshadowpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.foilageprepasspipeline == 0)
  {
    //
    // Foilage Prepass Pipeline
    //

    auto vs = assets.find(CoreAsset::foilage_prepass_vert);
    auto fs = assets.find(CoreAsset::prepass_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.prepass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.foilageprepasspipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.foilagegeometrypipeline == 0)
  {
    //
    // Foilage Pipeline
    //

    auto vs = assets.find(CoreAsset::foilage_geometry_vert);
    auto fs = assets.find(CoreAsset::geometry_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[3] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_EQUAL;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.geometrypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.foilagegeometrypipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.terrainprepasspipeline == 0)
  {
    //
    // Terrain Prepass Pipeline
    //

    auto vs = assets.find(CoreAsset::model_prepass_vert);
    auto fs = assets.find(CoreAsset::prepass_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[4] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, False), sizeof(ComputeConstants::False) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.prepass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.terrainprepasspipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.terraingeometrypipeline == 0)
  {
    //
    // Terrain Geometry Pipeline
    //

    auto vs = assets.find(CoreAsset::terrain_vert);
    auto fs = assets.find(CoreAsset::terrain_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[3] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_EQUAL;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[5] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::CutOut, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };
    specializationmap[4] = { ShaderLocation::DecalMask, offsetof(ComputeConstants, One), sizeof(ComputeConstants::One) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.geometrypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.terraingeometrypipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.depthmippipeline[0] == 0)
  {
    //
    // Depth Mip Pipeline
    //

    auto cs = assets.find(CoreAsset::depth_mip_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    for(size_t i = 0; i < extentof(context.depthmippipeline); ++i)
    {
      VkSpecializationMapEntry specializationmap[3] = {};
      specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, DepthMipSizeX), sizeof(ComputeConstants::DepthMipSizeX) };
      specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, DepthMipSizeY), sizeof(ComputeConstants::DepthMipSizeY) };
      specializationmap[2] = { ShaderLocation::DepthMipLayer, uint32_t(offsetof(ComputeConstants, Layers) + i*sizeof(uint32_t)), sizeof(uint32_t) };

      VkSpecializationInfo specializationinfo = {};
      specializationinfo.mapEntryCount = extentof(specializationmap);
      specializationinfo.pMapEntries = specializationmap;
      specializationinfo.dataSize = sizeof(computeconstants);
      specializationinfo.pData = &computeconstants;

      VkComputePipelineCreateInfo pipelineinfo = {};
      pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
      pipelineinfo.layout = context.pipelinelayout;
      pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
      pipelineinfo.stage.module = csmodule;
      pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
      pipelineinfo.stage.pName = "main";

      context.depthmippipeline[i] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
    }
  }

  if (context.esmpipeline[0] == 0)
  {
    //
    // ESM Gen Pipeline
    //

    auto cs = assets.find(CoreAsset::esm_gen_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[2] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, ESMGenSizeX), sizeof(ComputeConstants::ESMGenSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, ESMGenSizeY), sizeof(ComputeConstants::ESMGenSizeY) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.esmpipeline[0] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.esmpipeline[1] == 0)
  {
    //
    // ESM H-Blur Pipeline
    //

    auto cs = assets.find(CoreAsset::esm_hblur_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[2] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, ESMHBlurSize), sizeof(ComputeConstants::ESMHBlurSize) };
    specializationmap[1] = { ShaderLocation::ESMBlurRadius, offsetof(ComputeConstants, ESMBlurRadius), sizeof(ComputeConstants::ESMBlurRadius) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.esmpipeline[1] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.esmpipeline[2] == 0)
  {
    //
    // ESM V-Blur Pipeline
    //

    auto cs = assets.find(CoreAsset::esm_vblur_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[2] = {};
    specializationmap[0] = { ShaderLocation::SizeY, offsetof(ComputeConstants, ESMVBlurSize), sizeof(ComputeConstants::ESMVBlurSize) };
    specializationmap[1] = { ShaderLocation::ESMBlurRadius, offsetof(ComputeConstants, ESMBlurRadius), sizeof(ComputeConstants::ESMBlurRadius) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.esmpipeline[2] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.fogvolumepipeline[0] == 0)
  {
    //
    // Volumetric Fog Density Pipeline
    //

    auto cs = assets.find(CoreAsset::fog_density_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[11] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, FogDensitySizeX), sizeof(ComputeConstants::FogDensitySizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, FogDensitySizeY), sizeof(ComputeConstants::FogDensitySizeY) };
    specializationmap[2] = { ShaderLocation::SizeZ, offsetof(ComputeConstants, FogDensitySizeZ), sizeof(ComputeConstants::FogDensitySizeZ) };
    specializationmap[3] = { ShaderLocation::FogVolumeX, offsetof(ComputeConstants, FogVolumeX), sizeof(ComputeConstants::FogVolumeX) };
    specializationmap[4] = { ShaderLocation::FogVolumeY, offsetof(ComputeConstants, FogVolumeY), sizeof(ComputeConstants::FogVolumeY) };
    specializationmap[5] = { ShaderLocation::FogVolumeZ, offsetof(ComputeConstants, FogVolumeZ), sizeof(ComputeConstants::FogVolumeZ) };
    specializationmap[6] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[7] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[8] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[9] = { ShaderLocation::FogDepthRange, offsetof(ComputeConstants, FogDepthRange), sizeof(ComputeConstants::FogDepthRange) };
    specializationmap[10] = { ShaderLocation::FogDepthExponent, offsetof(ComputeConstants, FogDepthExponent), sizeof(ComputeConstants::FogDepthExponent) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.fogvolumepipeline[0] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.fogvolumepipeline[1] == 0)
  {
    //
    // Volumetric Fog Scatter Pipeline
    //

    auto cs = assets.find(CoreAsset::fog_scatter_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[11] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, FogScatterSizeX), sizeof(ComputeConstants::FogScatterSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, FogScatterSizeY), sizeof(ComputeConstants::FogScatterSizeY) };
    specializationmap[2] = { ShaderLocation::SizeZ, offsetof(ComputeConstants, FogScatterSizeZ), sizeof(ComputeConstants::FogScatterSizeZ) };
    specializationmap[3] = { ShaderLocation::FogVolumeX, offsetof(ComputeConstants, FogVolumeX), sizeof(ComputeConstants::FogVolumeX) };
    specializationmap[4] = { ShaderLocation::FogVolumeY, offsetof(ComputeConstants, FogVolumeY), sizeof(ComputeConstants::FogVolumeY) };
    specializationmap[5] = { ShaderLocation::FogVolumeZ, offsetof(ComputeConstants, FogVolumeZ), sizeof(ComputeConstants::FogVolumeZ) };
    specializationmap[6] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[7] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[8] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[9] = { ShaderLocation::FogDepthRange, offsetof(ComputeConstants, FogDepthRange), sizeof(ComputeConstants::FogDepthRange) };
    specializationmap[10] = { ShaderLocation::FogDepthExponent, offsetof(ComputeConstants, FogDepthExponent), sizeof(ComputeConstants::FogDepthExponent) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.fogvolumepipeline[1] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.lightingpipeline == 0)
  {
    //
    // Lighting Pipeline
    //

    auto cs = assets.find(CoreAsset::lighting_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[7] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, LightingSizeX), sizeof(ComputeConstants::LightingSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, LightingSizeY), sizeof(ComputeConstants::LightingSizeY) };
    specializationmap[2] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[3] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[4] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[5] = { ShaderLocation::FogDepthRange, offsetof(ComputeConstants, FogDepthRange), sizeof(ComputeConstants::FogDepthRange) };
    specializationmap[6] = { ShaderLocation::FogDepthExponent, offsetof(ComputeConstants, FogDepthExponent), sizeof(ComputeConstants::FogDepthExponent) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.lightingpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.skyboxpipeline == 0)
  {
    //
    // SkyBox Pipeline
    //

    auto vs = assets.find(CoreAsset::skybox_vert);
    auto fs = assets.find(CoreAsset::skybox_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.subpass = 2;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.skyboxpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.ssaopipeline == 0)
  {
    //
    // SSAO Pipeline
    //

    auto cs = assets.find(CoreAsset::ssao_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[5] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, SSAOSizeX), sizeof(ComputeConstants::SSAOSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, SSAOSizeY), sizeof(ComputeConstants::SSAOSizeY) };
    specializationmap[2] = { ShaderLocation::NoiseSize, offsetof(ComputeConstants, NoiseSize), sizeof(ComputeConstants::NoiseSize) };
    specializationmap[3] = { ShaderLocation::KernelSize, offsetof(ComputeConstants, KernelSize), sizeof(ComputeConstants::KernelSize) };
    specializationmap[4] = { ShaderLocation::SSAORadius, offsetof(ComputeConstants, SSAORadius), sizeof(ComputeConstants::SSAORadius) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.ssaopipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);

    mt19937 random(random_device{}());
    uniform_real_distribution<float> unit(0.0f, 1.0f);

    SSAOSet ssaoset;

    for(size_t i = 0; i < extentof(ssaoset.noise); ++i)
    {
      ssaoset.noise[i] = Vec4(normalise(Vec2(2*unit(random)-1, 2*unit(random)-1)), unit(random), unit(random));
    }

    for(size_t i = 0; i < extentof(ssaoset.kernel); ++i)
    {
      ssaoset.kernel[i] = normalise(Vec4(2*unit(random)-1, 2*unit(random)-1, unit(random), 0.0f)) * lerp(0.1f, 1.0f, pow(i / (float)extentof(ssaoset.kernel), 2.0f));
    }

    context.ssaoset = create_storagebuffer(context.vulkan, sizeof(ssaoset), &ssaoset);
  }

  if (context.opaquepipeline == 0)
  {
    //
    // Opaque Pipeline
    //

    auto vs = assets.find(CoreAsset::opaque_vert);
    auto fs = assets.find(CoreAsset::opaque_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[3] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[5] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::FogDepthRange, offsetof(ComputeConstants, FogDepthRange), sizeof(ComputeConstants::FogDepthRange) };
    specializationmap[4] = { ShaderLocation::FogDepthExponent, offsetof(ComputeConstants, FogDepthExponent), sizeof(ComputeConstants::FogDepthExponent) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.subpass = 0;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.opaquepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.translucentpipeline == 0)
  {
    //
    // Translucent Pipeline
    //

    auto vs = assets.find(CoreAsset::translucent_vert);
    auto fs = assets.find(CoreAsset::translucent_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[5] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::FogDepthRange, offsetof(ComputeConstants, FogDepthRange), sizeof(ComputeConstants::FogDepthRange) };
    specializationmap[4] = { ShaderLocation::FogDepthExponent, offsetof(ComputeConstants, FogDepthExponent), sizeof(ComputeConstants::FogDepthExponent) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.subpass = 2;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.translucentpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.translucentblendpipeline == 0)
  {
    //
    // Translucent Blend Pipeline
    //

    auto vs = assets.find(CoreAsset::translucent_blend_vert);
    auto fs = assets.find(CoreAsset::translucent_blend_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[2] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].blendEnable = VK_TRUE;
    blendattachments[1].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[1].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[1].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendattachments[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[1].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[5] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::FogDepthRange, offsetof(ComputeConstants, FogDepthRange), sizeof(ComputeConstants::FogDepthRange) };
    specializationmap[4] = { ShaderLocation::FogDepthExponent, offsetof(ComputeConstants, FogDepthExponent), sizeof(ComputeConstants::FogDepthExponent) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.subpass = 1;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.translucentblendpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.fogplanepipeline == 0)
  {
    //
    // Fog Plane Pipeline
    //

    auto vs = assets.find(CoreAsset::fogplane_vert);
    auto fs = assets.find(CoreAsset::fogplane_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[1] = {};
    specializationmap[0] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.subpass = 2;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.fogplanepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.oceanpipeline == 0)
  {
    //
    // Ocean Pipeline
    //

    auto vs = assets.find(CoreAsset::ocean_vert);
    auto fs = assets.find(CoreAsset::ocean_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[3] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.geometrypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.oceanpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.particlepipeline == 0)
  {
    //
    // Particle Pipeline (Soft Additive)
    //

    auto vs = assets.find(CoreAsset::particle_vert);
    auto fs = assets.find(CoreAsset::particle_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[6] = {};
    specializationmap[0] = { ShaderLocation::SoftParticles, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };
    specializationmap[1] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[2] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[3] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[4] = { ShaderLocation::FogDepthRange, offsetof(ComputeConstants, FogDepthRange), sizeof(ComputeConstants::FogDepthRange) };
    specializationmap[5] = { ShaderLocation::FogDepthExponent, offsetof(ComputeConstants, FogDepthExponent), sizeof(ComputeConstants::FogDepthExponent) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.subpass = 2;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.particlepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.particleblendpipeline == 0)
  {
    //
    // Particle Pipeline (Soft Blend)
    //

    auto vs = assets.find(CoreAsset::particle_blend_vert);
    auto fs = assets.find(CoreAsset::particle_blend_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[2] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendattachments[1].blendEnable = VK_TRUE;
    blendattachments[1].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[1].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[1].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendattachments[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[1].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[6] = {};
    specializationmap[0] = { ShaderLocation::SoftParticles, offsetof(ComputeConstants, True), sizeof(ComputeConstants::True) };
    specializationmap[1] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[2] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[3] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[4] = { ShaderLocation::FogDepthRange, offsetof(ComputeConstants, FogDepthRange), sizeof(ComputeConstants::FogDepthRange) };
    specializationmap[5] = { ShaderLocation::FogDepthExponent, offsetof(ComputeConstants, FogDepthExponent), sizeof(ComputeConstants::FogDepthExponent) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.subpass = 1;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.particleblendpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.waterpipeline == 0)
  {
    //
    // Water Pipeline
    //

    auto vs = assets.find(CoreAsset::water_vert);
    auto fs = assets.find(CoreAsset::water_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[5] = {};
    specializationmap[0] = { ShaderLocation::ClusterTileX, offsetof(ComputeConstants, ClusterTileX), sizeof(ComputeConstants::ClusterTileX) };
    specializationmap[1] = { ShaderLocation::ClusterTileY, offsetof(ComputeConstants, ClusterTileY), sizeof(ComputeConstants::ClusterTileY) };
    specializationmap[2] = { ShaderLocation::ShadowSlices, offsetof(ComputeConstants, ShadowSlices), sizeof(ComputeConstants::ShadowSlices) };
    specializationmap[3] = { ShaderLocation::FogDepthRange, offsetof(ComputeConstants, FogDepthRange), sizeof(ComputeConstants::FogDepthRange) };
    specializationmap[4] = { ShaderLocation::FogDepthExponent, offsetof(ComputeConstants, FogDepthExponent), sizeof(ComputeConstants::FogDepthExponent) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.subpass = 2;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.waterpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.weightblendpipeline == 0)
  {
    //
    // Weight Blend Pipeline
    //

    auto vs = assets.find(CoreAsset::weightblend_vert);
    auto fs = assets.find(CoreAsset::weightblend_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.forwardpass;
    pipelineinfo.subpass = 2;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.weightblendpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.ssrpipeline == 0)
  {
    //
    // SSR Gen Pipeline
    //

    auto cs = assets.find(CoreAsset::ssr_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[2] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, SSRSizeX), sizeof(ComputeConstants::SSRSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, SSRSizeY), sizeof(ComputeConstants::SSRSizeY) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.ssrpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.luminancepipeline == 0)
  {
    //
    // Luminance Pipeline
    //

    auto cs = assets.find(CoreAsset::luminance_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

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

    context.luminancepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.bloompipeline[0] == 0)
  {
    //
    // Bloom Luma Pipeline
    //

    auto cs = assets.find(CoreAsset::bloom_luma_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, BloomLumaSizeX), sizeof(ComputeConstants::BloomLumaSizeX) };
    specializationmap[1] = { ShaderLocation::SizeY, offsetof(ComputeConstants, BloomLumaSizeY), sizeof(ComputeConstants::BloomLumaSizeY) };
    specializationmap[2] = { ShaderLocation::BloomCutoff, offsetof(ComputeConstants, BloomCutoff), sizeof(ComputeConstants::BloomCutoff) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.bloompipeline[0] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.bloompipeline[1] == 0)
  {
    //
    // Bloom H-Blur Pipeline
    //

    auto cs = assets.find(CoreAsset::bloom_hblur_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, BloomHBlurSize), sizeof(ComputeConstants::BloomHBlurSize) };
    specializationmap[1] = { ShaderLocation::BloomBlurSigma, offsetof(ComputeConstants, BloomBlurSigma), sizeof(ComputeConstants::BloomBlurSigma) };
    specializationmap[2] = { ShaderLocation::BloomBlurRadius, offsetof(ComputeConstants, BloomBlurRadius), sizeof(ComputeConstants::BloomBlurRadius) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.bloompipeline[1] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.bloompipeline[2] == 0)
  {
    //
    // Bloom V-Blur Pipeline
    //

    auto cs = assets.find(CoreAsset::bloom_vblur_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::SizeY, offsetof(ComputeConstants, BloomVBlurSize), sizeof(ComputeConstants::BloomVBlurSize) };
    specializationmap[1] = { ShaderLocation::BloomBlurSigma, offsetof(ComputeConstants, BloomBlurSigma), sizeof(ComputeConstants::BloomBlurSigma) };
    specializationmap[2] = { ShaderLocation::BloomBlurRadius, offsetof(ComputeConstants, BloomBlurRadius), sizeof(ComputeConstants::BloomBlurRadius) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.bloompipeline[2] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.colorblurpipeline[0] == 0)
  {
    //
    // Color H-Blur Pipeline
    //

    auto cs = assets.find(CoreAsset::color_hblur_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::SizeX, offsetof(ComputeConstants, ColorHBlurSize), sizeof(ComputeConstants::ColorHBlurSize) };
    specializationmap[1] = { ShaderLocation::ColorBlurSigma, offsetof(ComputeConstants, ColorBlurSigma), sizeof(ComputeConstants::ColorBlurSigma) };
    specializationmap[2] = { ShaderLocation::ColorBlurRadius, offsetof(ComputeConstants, ColorBlurRadius), sizeof(ComputeConstants::ColorBlurRadius) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.colorblurpipeline[0] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.colorblurpipeline[1] == 0)
  {
    //
    // Color V-Blur Pipeline
    //

    auto cs = assets.find(CoreAsset::color_vblur_comp);

    if (!cs)
      return false;

    asset_guard lock(assets);

    auto cssrc = assets.request(platform, cs);

    if (!cssrc)
      return false;

    auto csmodule = create_shadermodule(context.vulkan, cssrc, cs->length);

    VkSpecializationMapEntry specializationmap[3] = {};
    specializationmap[0] = { ShaderLocation::SizeY, offsetof(ComputeConstants, ColorVBlurSize), sizeof(ComputeConstants::ColorVBlurSize) };
    specializationmap[1] = { ShaderLocation::ColorBlurSigma, offsetof(ComputeConstants, ColorBlurSigma), sizeof(ComputeConstants::ColorBlurSigma) };
    specializationmap[2] = { ShaderLocation::ColorBlurRadius, offsetof(ComputeConstants, ColorBlurRadius), sizeof(ComputeConstants::ColorBlurRadius) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkComputePipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineinfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineinfo.stage.module = csmodule;
    pipelineinfo.stage.pSpecializationInfo = &specializationinfo;
    pipelineinfo.stage.pName = "main";

    context.colorblurpipeline[1] = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.compositepipeline == 0)
  {
    //
    // Composite Pipeline
    //

    auto vs = assets.find(CoreAsset::composite_vert);
    auto fs = assets.find(CoreAsset::composite_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkSpecializationMapEntry specializationmap[2] = {};
    specializationmap[0] = { ShaderLocation::DepthOfField, offsetof(ComputeConstants, DepthOfField), sizeof(ComputeConstants::DepthOfField) };
    specializationmap[1] = { ShaderLocation::ColorGrading, offsetof(ComputeConstants, ColorGrading), sizeof(ComputeConstants::ColorGrading) };

    VkSpecializationInfo specializationinfo = {};
    specializationinfo.mapEntryCount = extentof(specializationmap);
    specializationinfo.pMapEntries = specializationmap;
    specializationinfo.dataSize = sizeof(computeconstants);
    specializationinfo.pData = &computeconstants;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pSpecializationInfo = &specializationinfo;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pSpecializationInfo = &specializationinfo;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.overlaypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.compositepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);

    context.compositecommands[0] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    context.compositecommands[1] = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
  }

  if (context.spritepipeline == 0)
  {
    //
    // Sprite Pipeline
    //

    auto vs = assets.find(CoreAsset::sprite_vert);
    auto fs = assets.find(CoreAsset::sprite_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.overlaypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.spritepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.gizmopipeline == 0)
  {
    //
    // Gizmo Pipeline
    //

    auto vs = assets.find(CoreAsset::gizmo_vert);
    auto fs = assets.find(CoreAsset::gizmo_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_TRUE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.overlaypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.gizmopipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.wireframepipeline == 0)
  {
    //
    // WireFrame Pipeline
    //

    auto vs = assets.find(CoreAsset::wireframe_vert);
    auto gs = assets.find(CoreAsset::wireframe_geom);
    auto fs = assets.find(CoreAsset::wireframe_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.overlaypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.wireframepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.stencilmaskpipeline == 0)
  {
    //
    // Stencil Mask Pipeline
    //

    auto vs = assets.find(CoreAsset::stencilmask_vert);
    auto fs = assets.find(CoreAsset::stencilmask_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_FALSE;
    blendattachments[0].colorWriteMask = 0;//VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_FALSE;
    depthstate.stencilTestEnable = VK_TRUE;
    depthstate.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depthstate.front.passOp = VK_STENCIL_OP_REPLACE;
    depthstate.front.writeMask = 0xFF;
    depthstate.front.reference = 1;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.overlaypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.stencilmaskpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.stencilfillpipeline == 0)
  {
    //
    // Stencil Fill Pipeline
    //

    auto vs = assets.find(CoreAsset::stencilfill_vert);
    auto fs = assets.find(CoreAsset::stencilfill_frag);

    if (!vs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;
    depthstate.stencilTestEnable = VK_TRUE;
    depthstate.front.compareOp = VK_COMPARE_OP_EQUAL;
    depthstate.front.compareMask = 0xFF;
    depthstate.front.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
    depthstate.front.writeMask = 0xFF;
    depthstate.front.reference = 1;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[2] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fsmodule;
    shaders[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.overlaypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.stencilfillpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.stencilpathpipeline == 0)
  {
    //
    // Stencil Path Pipeline
    //

    auto vs = assets.find(CoreAsset::stencilpath_vert);
    auto gs = assets.find(CoreAsset::stencilpath_geom);
    auto fs = assets.find(CoreAsset::stencilpath_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;
    depthstate.stencilTestEnable = VK_TRUE;
    depthstate.front.compareOp = VK_COMPARE_OP_GREATER;
    depthstate.front.compareMask = 0xFF;
    depthstate.front.passOp = VK_STENCIL_OP_REPLACE;
    depthstate.front.writeMask = 0xFF;
    depthstate.front.reference = 1;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.overlaypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.stencilpathpipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.linepipeline == 0)
  {
    //
    // Line Pipeline
    //

    auto vs = assets.find(CoreAsset::line_vert);
    auto gs = assets.find(CoreAsset::line_geom);
    auto fs = assets.find(CoreAsset::line_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
//    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.overlaypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.linepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.outlinepipeline == 0)
  {
    //
    // Outline Pipeline
    //

    auto vs = assets.find(CoreAsset::outline_vert);
    auto gs = assets.find(CoreAsset::outline_geom);
    auto fs = assets.find(CoreAsset::outline_frag);

    if (!vs || !gs || !fs)
      return false;

    asset_guard lock(assets);

    auto vssrc = assets.request(platform, vs);
    auto gssrc = assets.request(platform, gs);
    auto fssrc = assets.request(platform, fs);

    if (!vssrc || !gssrc || !fssrc)
      return false;

    auto vsmodule = create_shadermodule(context.vulkan, vssrc, vs->length);
    auto gsmodule = create_shadermodule(context.vulkan, gssrc, gs->length);
    auto fsmodule = create_shadermodule(context.vulkan, fssrc, fs->length);

    VkVertexInputBindingDescription vertexbindings[1] = {};
    vertexbindings[0].stride = VertexLayout::stride;
    vertexbindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexinput = {};
    vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexinput.vertexBindingDescriptionCount = extentof(vertexbindings);
    vertexinput.pVertexBindingDescriptions = vertexbindings;
    vertexinput.vertexAttributeDescriptionCount = extentof(context.vertexattributes);
    vertexinput.pVertexAttributeDescriptions = context.vertexattributes;

    VkPipelineInputAssemblyStateCreateInfo inputassembly = {};
    inputassembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0;

    VkPipelineColorBlendAttachmentState blendattachments[1] = {};
    blendattachments[0].blendEnable = VK_TRUE;
    blendattachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].colorBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendattachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendattachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    blendattachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorblend = {};
    colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend.attachmentCount = extentof(blendattachments);
    colorblend.pAttachments = blendattachments;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthstate = {};
    depthstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstate.depthTestEnable = VK_TRUE;
    depthstate.depthWriteEnable = VK_FALSE;
    depthstate.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkDynamicState dynamicstates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic = {};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = extentof(dynamicstates);
    dynamic.pDynamicStates = dynamicstates;

    VkPipelineShaderStageCreateInfo shaders[3] = {};
    shaders[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vsmodule;
    shaders[0].pName = "main";
    shaders[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    shaders[1].module = gsmodule;
    shaders[1].pName = "main";
    shaders[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[2].module = fsmodule;
    shaders[2].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineinfo = {};
    pipelineinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineinfo.layout = context.pipelinelayout;
    pipelineinfo.renderPass = context.overlaypass;
    pipelineinfo.pVertexInputState = &vertexinput;
    pipelineinfo.pInputAssemblyState = &inputassembly;
    pipelineinfo.pRasterizationState = &rasterization;
    pipelineinfo.pColorBlendState = &colorblend;
    pipelineinfo.pMultisampleState = &multisample;
    pipelineinfo.pDepthStencilState = &depthstate;
    pipelineinfo.pViewportState = &viewport;
    pipelineinfo.pDynamicState = &dynamic;
    pipelineinfo.stageCount = extentof(shaders);
    pipelineinfo.pStages = shaders;

    context.outlinepipeline = create_pipeline(context.vulkan, context.pipelinecache, pipelineinfo);
  }

  if (context.envbrdf == 0)
  {
    auto image = assets.find(CoreAsset::envbrdf_lut);

    if (!image)
      return false;

    asset_guard lock(assets);

    auto bits = assets.request(platform, image);

    if (!bits)
      return false;

    context.envbrdf = create_texture(context.vulkan, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, bits, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
  }

  if (context.whitediffuse == 0)
  {
    auto image = assets.find(CoreAsset::white_diffuse);

    if (!image)
      return false;

    asset_guard lock(assets);

    auto bits = assets.request(platform, image);

    if (!bits)
      return false;

    context.whitediffuse = create_texture(context.vulkan, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, bits);
  }

  if (context.nominalnormal == 0)
  {
    auto image = assets.find(CoreAsset::nominal_normal);

    if (!image)
      return false;

    asset_guard lock(assets);

    auto bits = assets.request(platform, image);

    if (!bits)
      return false;

    context.nominalnormal = create_texture(context.vulkan, context.transferbuffer, image->width, image->height, image->layers, image->levels, VK_FORMAT_B8G8R8A8_UNORM, bits);
  }

  if (context.unitquad == 0)
  {
    auto mesh = assets.find(CoreAsset::unit_quad);

    if (!mesh)
      return false;

    asset_guard lock(assets);

    auto bits = assets.request(platform, mesh);

    if (!bits)
      return false;

    context.unitquad = create_vertexbuffer(context.vulkan, context.transferbuffer, bits, mesh->vertexcount, sizeof(PackVertex));
  }

  context.width = 0;
  context.height = 0;

  context.prepared = true;

  return true;
}


///////////////////////// prepare_render_pipeline /////////////////////////
void prepare_render_pipeline(RenderContext &context, RenderParams const &params)
{
  assert(context.prepared);

  if (context.width != params.width || context.height != params.height || context.scale != params.scale || context.aspect != params.aspect || context.ssaoscale != params.ssaoscale)
  {
    context.ready = false;

    vkDeviceWaitIdle(context.vulkan);
    vkResetCommandBuffer(context.commandbuffers[0], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    vkResetCommandBuffer(context.commandbuffers[1], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    CommandPool setuppool = create_commandpool(context.vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    CommandBuffer setupbuffer = allocate_commandbuffer(context.vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    begin(context.vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (context.width != params.width || context.height != params.height || context.scale != params.scale || context.aspect != params.aspect)
    {
      release_render_pipeline(context);

      context.width = params.width;
      context.height = params.height;
      context.scale = params.scale;
      context.aspect = params.aspect;

      context.fbox = (context.width - min((int)(context.height * params.aspect), context.width))/2;
      context.fboy = (context.height - min((int)(context.width / params.aspect), context.height))/2;
      context.fbowidth = (int)(2*(context.width/2 - context.fbox) * context.scale);
      context.fboheight = (int)(2*(context.height/2 - context.fboy) * context.scale);

      assert(context.fbowidth != 0 && context.fboheight != 0);

#ifndef NDEBUG
      const int ClusterSize = (24 + 24 + 24*512/32 + 24 + 24*512/32 + 24 + 24*128/32 + 24 + 24*128/32) * sizeof(uint32_t);
      assert(sizeof(SceneSet) + (context.fbowidth/computeconstants.ClusterTileX+1)*(context.fboheight/computeconstants.ClusterTileY+1)*ClusterSize < SceneBufferSize);
#endif

      //
      // Shadow Map
      //

      context.shadows.shadowmap = create_texture(context.vulkan, setupbuffer, context.shadows.width, context.shadows.height, context.shadows.nslices, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

      VkSamplerCreateInfo shadowsamplerinfo = {};
      shadowsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      shadowsamplerinfo.magFilter = VK_FILTER_LINEAR;
      shadowsamplerinfo.minFilter = VK_FILTER_LINEAR;
      shadowsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      shadowsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      shadowsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      shadowsamplerinfo.compareEnable = VK_TRUE;
      shadowsamplerinfo.compareOp = VK_COMPARE_OP_LESS;

      context.shadows.shadowmap.sampler = create_sampler(context.vulkan, shadowsamplerinfo);

      context.esmshadowbuffer = create_texture(context.vulkan, setupbuffer, context.shadows.width/4, context.shadows.height/4, 1, 1, VK_FORMAT_R32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

      //
      // Shadow Frame Buffer
      //

      VkImageView shadowbuffer[1] = {};
      shadowbuffer[0] = context.shadows.shadowmap.imageview;

      VkFramebufferCreateInfo shadowbufferinfo = {};
      shadowbufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      shadowbufferinfo.renderPass = context.shadowpass;
      shadowbufferinfo.attachmentCount = extentof(shadowbuffer);
      shadowbufferinfo.pAttachments = shadowbuffer;
      shadowbufferinfo.width = context.shadows.width;
      shadowbufferinfo.height = context.shadows.height;
      shadowbufferinfo.layers = context.shadows.nslices;

      context.shadowbuffer = create_framebuffer(context.vulkan, shadowbufferinfo);

      //
      // Color Attachment
      //

      context.colorbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 2, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

      for(uint32_t i = 0; i < extentof(context.colormipviews); ++i)
      {
        VkImageViewCreateInfo viewinfo = {};
        viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewinfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1};
        viewinfo.image = context.colorbuffer.image;

        context.colormipviews[i] = create_imageview(context.vulkan, viewinfo);
      }

      //
      // Geometry Attachment
      //

      context.diffusebuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
      context.specularbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
      context.normalbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

      //
      // Depth Attachment
      //

      context.depthbuffer = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

      VkSamplerCreateInfo depthsamplerinfo = {};
      depthsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      depthsamplerinfo.magFilter = VK_FILTER_LINEAR;
      depthsamplerinfo.minFilter = VK_FILTER_LINEAR;
      depthsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
      depthsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
      depthsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
      depthsamplerinfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

      context.depthbuffer.sampler = create_sampler(context.vulkan, depthsamplerinfo);

      // Depth Hi-Z

      uint32_t depthlevels = 6;

      context.depthmipbuffer = create_texture(context.vulkan, setupbuffer, ((context.fbowidth >> depthlevels) + 1) << (depthlevels-1), ((context.fboheight >> depthlevels) + 1) << (depthlevels-1), 1, depthlevels, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

      for(uint32_t i = 0; i < extentof(context.depthmipviews); ++i)
      {
        VkImageViewCreateInfo viewinfo = {};
        viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewinfo.format = VK_FORMAT_R16G16_SFLOAT;
        viewinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1};
        viewinfo.image = context.depthmipbuffer.image;

        context.depthmipviews[i] = create_imageview(context.vulkan, viewinfo);
      }

      //
      // Scratch Buffers
      //

      context.scratchbuffers[0] = create_texture(context.vulkan, setupbuffer, context.fbowidth/2, context.fboheight/2, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
      context.scratchbuffers[1] = create_texture(context.vulkan, setupbuffer, context.fbowidth/2, context.fboheight/2, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
      context.scratchbuffers[2] = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
      context.scratchbuffers[3] = create_texture(context.vulkan, setupbuffer, context.fbowidth, context.fboheight, 1, 1, VK_FORMAT_R32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

      for(size_t i = 0; i < extentof(context.scratchbuffers); ++i)
      {
        clear(setupbuffer, context.scratchbuffers[i].image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Color4(0.0f, 0.0f, 0.0f, 0.0f));
      }

      //
      // Prepass Frame Buffer
      //

      VkImageView prebuffer[1] = {};
      prebuffer[0] = context.depthbuffer.imageview;

      VkFramebufferCreateInfo prebufferinfo = {};
      prebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      prebufferinfo.renderPass = context.prepass;
      prebufferinfo.attachmentCount = extentof(prebuffer);
      prebufferinfo.pAttachments = prebuffer;
      prebufferinfo.width = context.fbowidth;
      prebufferinfo.height = context.fboheight;
      prebufferinfo.layers = 1;

      context.preframebuffer = create_framebuffer(context.vulkan, prebufferinfo);

      //
      // Geometry Frame Buffer
      //

      VkImageView geometrybuffer[4] = {};
      geometrybuffer[0] = context.diffusebuffer.imageview;
      geometrybuffer[1] = context.specularbuffer.imageview;
      geometrybuffer[2] = context.normalbuffer.imageview;
      geometrybuffer[3] = context.depthbuffer.imageview;

      VkFramebufferCreateInfo geometrybufferinfo = {};
      geometrybufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      geometrybufferinfo.renderPass = context.geometrypass;
      geometrybufferinfo.attachmentCount = extentof(geometrybuffer);
      geometrybufferinfo.pAttachments = geometrybuffer;
      geometrybufferinfo.width = context.fbowidth;
      geometrybufferinfo.height = context.fboheight;
      geometrybufferinfo.layers = 1;

      context.geometryframebuffer = create_framebuffer(context.vulkan, geometrybufferinfo);

      //
      // Forward Frame Buffer
      //

      VkImageView forwardbuffer[6] = {};
      forwardbuffer[0] = context.colormipviews[0];
      forwardbuffer[1] = context.specularbuffer.imageview;
      forwardbuffer[2] = context.normalbuffer.imageview;
      forwardbuffer[3] = context.scratchbuffers[2].imageview;
      forwardbuffer[4] = context.scratchbuffers[3].imageview;
      forwardbuffer[5] = context.depthbuffer.imageview;

      VkFramebufferCreateInfo forwardbufferinfo = {};
      forwardbufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      forwardbufferinfo.renderPass = context.forwardpass;
      forwardbufferinfo.attachmentCount = extentof(forwardbuffer);
      forwardbufferinfo.pAttachments = forwardbuffer;
      forwardbufferinfo.width = context.fbowidth;
      forwardbufferinfo.height = context.fboheight;
      forwardbufferinfo.layers = 1;

      context.forwardframebuffer = create_framebuffer(context.vulkan, forwardbufferinfo);

      //
      // Render Target
      //

      context.rendertarget = create_texture(context.vulkan, setupbuffer, context.width, context.height, 1, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
      context.depthstencil = create_texture(context.vulkan, setupbuffer, context.width, context.height, 1, 1, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

      //
      // Frame Buffer
      //

      VkImageView framebuffer[2] = {};
      framebuffer[0] = context.rendertarget.imageview;
      framebuffer[1] = context.depthstencil.imageview;

      VkFramebufferCreateInfo framebufferinfo = {};
      framebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferinfo.renderPass = context.overlaypass;
      framebufferinfo.attachmentCount = extentof(framebuffer);
      framebufferinfo.pAttachments = framebuffer;
      framebufferinfo.width = context.width;
      framebufferinfo.height = context.height;
      framebufferinfo.layers = 1;

      context.framebuffer = create_framebuffer(context.vulkan, framebufferinfo);
    }

    //
    // SSAO
    //

    for(size_t i = 0; i < extentof(context.ssaobuffers); ++i)
    {
      context.ssaobuffers[i] = create_texture(context.vulkan, setupbuffer, max(int(context.fbowidth*params.ssaoscale), 1), max(int(context.fboheight*params.ssaoscale), 1), 1, 1, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

      clear(setupbuffer, context.ssaobuffers[i].image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Color4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    context.ssaoscale = params.ssaoscale;

    //
    // Volumetric Fog
    //

    for(size_t i = 0; i < extentof(context.fogvolumebuffers); ++i)
    {
      context.fogvolumebuffers[i] = create_texture(context.vulkan, setupbuffer, computeconstants.FogVolumeX, computeconstants.FogVolumeY, computeconstants.FogVolumeZ, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_VIEW_TYPE_3D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

      clear(setupbuffer, context.fogvolumebuffers[i].image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Color4(0.0f, 0.0f, 0.0f, (params.fogdensity == 0) ? 1.0f : 0.0f));
    }

    //
    // Scene Descriptor
    //

    bind_buffer(context.vulkan, context.scenedescriptor, ShaderLocation::scenebuf, context.sceneset, 0, context.sceneset.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::colormap, context.colorbuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::diffusemap, context.diffusebuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::specularmap, context.specularbuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::normalmap, context.normalbuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::depthmap, context.depthbuffer);
    bind_texture(context.vulkan, context.scenedescriptor, ShaderLocation::depthmipmap, context.depthmipbuffer);

    //
    // Frame Descriptors
    //

    for(size_t i = 0; i < extentof(context.framedescriptors); ++i)
    {
      context.framedescriptors[i] = {};
      context.framedescriptors[i] = allocate_descriptorset(context.vulkan, context.descriptorpool, context.scenesetlayout);

      bind_buffer(context.vulkan, context.framedescriptors[i], ShaderLocation::scenebuf, context.sceneset, 0, context.sceneset.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::colormap, context.colorbuffer);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::diffusemap, context.diffusebuffer);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::specularmap, context.specularbuffer);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::normalmap, context.normalbuffer);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmap, context.depthbuffer);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmipmap, context.depthmipbuffer);

      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::colortarget, context.colorbuffer);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::colormiptarget, context.colormipviews[1], VK_IMAGE_LAYOUT_GENERAL, 0);

      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmipsrc, context.depthmipbuffer.imageview, context.depthmipbuffer.sampler, VK_IMAGE_LAYOUT_GENERAL);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[0], VK_IMAGE_LAYOUT_GENERAL, 0);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[1], VK_IMAGE_LAYOUT_GENERAL, 1);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[2], VK_IMAGE_LAYOUT_GENERAL, 2);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[3], VK_IMAGE_LAYOUT_GENERAL, 3);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[4], VK_IMAGE_LAYOUT_GENERAL, 4);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::depthmiptarget, context.depthmipviews[5], VK_IMAGE_LAYOUT_GENERAL, 5);
      bind_attachment(context.vulkan, context.framedescriptors[i], ShaderLocation::depthattachment, context.depthbuffer.imageview, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::esmmap, context.esmshadowbuffer);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::esmtarget, context.esmshadowbuffer);

      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaomap, context.ssaobuffers[i]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::shadowmap, context.shadows.shadowmap);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::envbrdf, context.envbrdf);

      bind_buffer(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaobuf, context.ssaoset, 0, sizeof(SSAOSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaoprevmap, context.ssaobuffers[i ^ 1]);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::ssaotarget, context.ssaobuffers[i]);

      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::fogmap, context.fogvolumebuffers[i]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::fogdensitymap, context.fogvolumebuffers[i ^ 1]);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::fogdensitytarget, context.fogvolumebuffers[i ^ 1]);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::fogscattertarget, context.fogvolumebuffers[i]);

      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchmap0, context.scratchbuffers[0]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchmap1, context.scratchbuffers[1]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchmap2, context.scratchbuffers[2]);
      bind_texture(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchmap3, context.scratchbuffers[3]);

      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchtarget0, context.scratchbuffers[0]);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchtarget1, context.scratchbuffers[1]);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchtarget2, context.scratchbuffers[2]);
      bind_image(context.vulkan, context.framedescriptors[i], ShaderLocation::scratchtarget3, context.scratchbuffers[3]);

      bind_buffer(context.vulkan, context.framedescriptors[i], ShaderLocation::lumabuf, context.transferbuffer, 0, sizeof(LumaSet), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

#if 1 // Shut up the validation layer on unused maps
    auto tmp = create_texture(context.vulkan, setupbuffer, 1, 1, 6, 1, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    VkDescriptorImageInfo envmapinfos[8] = {};
    for(size_t i = 0; i < extentof(envmapinfos); ++i)
    {
      envmapinfos[i].sampler = tmp.sampler;
      envmapinfos[i].imageView = tmp.imageview;
      envmapinfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    bind_texture(context.vulkan, context.framedescriptors[0], ShaderLocation::envmaps, envmapinfos, extentof(envmapinfos));
    bind_texture(context.vulkan, context.framedescriptors[1], ShaderLocation::envmaps, envmapinfos, extentof(envmapinfos));

    tmp.image.release();
    tmp.sampler.release();
    tmp.imageview.release();

    tmp = create_texture(context.vulkan, setupbuffer, 1, 1, 1, 1, VK_FORMAT_R32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    VkDescriptorImageInfo spotmapinfos[16] = {};
    for(size_t i = 0; i < extentof(spotmapinfos); ++i)
    {
      spotmapinfos[i].sampler = tmp.sampler;
      spotmapinfos[i].imageView = tmp.imageview;
      spotmapinfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    bind_texture(context.vulkan, context.framedescriptors[0], ShaderLocation::spotmaps, spotmapinfos, extentof(spotmapinfos));
    bind_texture(context.vulkan, context.framedescriptors[1], ShaderLocation::spotmaps, spotmapinfos, extentof(spotmapinfos));

    tmp.image.release();
    tmp.sampler.release();
    tmp.imageview.release();

    VkDescriptorImageInfo decalmapinfos[16] = {};
    for(size_t i = 0; i < extentof(decalmapinfos); ++i)
    {
      decalmapinfos[i].sampler = context.whitediffuse.sampler;
      decalmapinfos[i].imageView = context.whitediffuse.imageview;
      decalmapinfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    bind_texture(context.vulkan, context.framedescriptors[0], ShaderLocation::decalmaps, decalmapinfos, extentof(decalmapinfos));
    bind_texture(context.vulkan, context.framedescriptors[1], ShaderLocation::decalmaps, decalmapinfos, extentof(decalmapinfos));

    tmp.image.release();
    tmp.sampler.release();
    tmp.imageview.release();

    tmp = create_texture(context.vulkan, setupbuffer, 32, 32, 32, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_VIEW_TYPE_3D, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    bind_texture(context.vulkan, context.framedescriptors[0], ShaderLocation::colorlut, tmp);
    bind_texture(context.vulkan, context.framedescriptors[1], ShaderLocation::colorlut, tmp);

    tmp.image.release();
    tmp.sampler.release();
    tmp.imageview.release();
#endif

    // Finalise

    end(context.vulkan, setupbuffer);

    submit(context.vulkan, setupbuffer);

    vkQueueWaitIdle(context.vulkan.queue);

    context.ready = true;
  }
}


///////////////////////// release_render_pipeline ///////////////////////////
void release_render_pipeline(RenderContext &context)
{
  vkDeviceWaitIdle(context.vulkan);
  vkResetCommandBuffer(context.commandbuffers[0], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  vkResetCommandBuffer(context.commandbuffers[1], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

  context.ready = false;

  context.shadowbuffer = {};

  context.rendertarget = {};
  context.depthstencil = {};

  context.colorbuffer = {};
  context.diffusebuffer = {};
  context.specularbuffer = {};
  context.normalbuffer = {};
  context.depthbuffer = {};
  context.depthstencil = {};

  context.preframebuffer = {};
  context.geometryframebuffer = {};
  context.forwardframebuffer = {};

  context.scratchbuffers[0] = {};
  context.scratchbuffers[1] = {};
  context.scratchbuffers[2] = {};

  context.ssaobuffers[0] = {};
  context.ssaobuffers[1] = {};

  context.shadows.shadowmap = {};

  context.width = 0;
  context.height = 0;
}


///////////////////////// blit //////////////////////////////////////////////
void blit(RenderContext &context, Vulkan::Texture const &src, VkBuffer dst, VkDeviceSize offset, VkSemaphore const (&dependancies)[8])
{
  VkImageSubresourceLayers srclayers = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, src.layers };

  if (src.format == VK_FORMAT_D16_UNORM || src.format == VK_FORMAT_D32_SFLOAT)
    srclayers.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  if (src.format == VK_FORMAT_D16_UNORM_S8_UINT || src.format == VK_FORMAT_D24_UNORM_S8_UINT || src.format == VK_FORMAT_D32_SFLOAT_S8_UINT)
    srclayers.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

  auto commandbuffer = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  setimagelayout(commandbuffer, src, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  blit(commandbuffer, src.image, 0, 0, src.width, src.height, srclayers, dst, offset);

  setimagelayout(commandbuffer, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  barrier(commandbuffer);

  end(context.vulkan, commandbuffer);

  auto fence = create_fence(context.vulkan);

  submit(context.vulkan, commandbuffer, fence, dependancies);

  wait_fence(context.vulkan, fence);
}


///////////////////////// blit //////////////////////////////////////////////
void blit(RenderContext &context, Vulkan::Texture const &src, VkImage dst, int dx, int dy, int dw, int dh, VkImageSubresourceLayers dstlayers, VkFilter filter, VkSemaphore const (&dependancies)[8])
{
  VkImageSubresourceLayers srclayers = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, src.layers };

  if (src.format == VK_FORMAT_D16_UNORM || src.format == VK_FORMAT_D32_SFLOAT)
    srclayers.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  if (src.format == VK_FORMAT_D16_UNORM_S8_UINT || src.format == VK_FORMAT_D24_UNORM_S8_UINT || src.format == VK_FORMAT_D32_SFLOAT_S8_UINT)
    srclayers.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

  auto commandbuffer = allocate_commandbuffer(context.vulkan, context.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  setimagelayout(commandbuffer, src, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  setimagelayout(commandbuffer, dst, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { dstlayers.aspectMask, dstlayers.mipLevel, 1, dstlayers.baseArrayLayer, dstlayers.layerCount });

  blit(commandbuffer, src.image, 0, 0, src.width, src.height, srclayers, dst, dx, dy, dw, dh, dstlayers, filter);

  setimagelayout(commandbuffer, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { dstlayers.aspectMask, dstlayers.mipLevel, 1, dstlayers.baseArrayLayer, dstlayers.layerCount });

  setimagelayout(commandbuffer, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  end(context.vulkan, commandbuffer);

  auto fence = create_fence(context.vulkan);

  submit(context.vulkan, commandbuffer, fence, dependancies);

  wait_fence(context.vulkan, fence);
}


///////////////////////// prepare_shadowview ////////////////////////////////
void prepare_shadowview(ShadowMap &shadowmap, Camera const &camera, Vec3 const &lightdirection)
{
  const float lambda = shadowmap.shadowsplitlambda;
  const float znear = 0.1f;
  const float zfar = shadowmap.shadowsplitfar;
  const float extrusion = 1000.0f;
  constexpr int nsplits = ShadowMap::nslices;

  float splits[nsplits+1] = { znear };

  for(int i = 1; i <= nsplits; ++i)
  {
    float alpha = (float)i / nsplits;
    float logdist = znear * pow(zfar / znear, alpha);
    float uniformdist = znear + (zfar -znear) * alpha;

    splits[i] = lerp(uniformdist, logdist, lambda);
  }

  auto up = Vec3(0, 1, 0);

  auto snapview = Transform::lookat(Vec3(0,0,0), -lightdirection, up);

  for(int i = 0; i < nsplits; ++i)
  {
    auto frustum = camera.frustum(splits[i], splits[i+1] + 1.0f);

    auto frustumradius = 0.5f * norm(frustum.corners[0] - frustum.corners[6]);

    auto frustumcentre = frustum.centre();

    frustumcentre = inverse(snapview) * frustumcentre;
    frustumcentre.x -= fmod(frustumcentre.x, (frustumradius + frustumradius) / shadowmap.width);
    frustumcentre.y -= fmod(frustumcentre.y, (frustumradius + frustumradius) / shadowmap.height);
    frustumcentre = snapview * frustumcentre;

    auto lightpos = frustumcentre - extrusion * lightdirection;

    auto lightview = Transform::lookat(lightpos, lightpos + lightdirection, up);

    auto lightproj = OrthographicProjection(-frustumradius, -frustumradius, frustumradius, frustumradius, 0.1f, extrusion + frustumradius) * ScaleMatrix(1.0f, -1.0f, 1.0f, 1.0f);

    shadowmap.splits[i] = splits[i+1];
    shadowmap.shadowview[i] = lightproj * inverse(lightview).matrix();
  }
}


///////////////////////// bind_decalmap /////////////////////////////////////
uint32_t bind_decalmap(RenderContext &context, Vulkan::Texture const &texture)
{
  auto &declmaps = context.decalmaps[context.frame & 1];

  uint32_t slot = 0;
  for(size_t i = 0; i < extentof(declmaps); ++i)
  {
    if (get<0>(declmaps[i]) < context.frame || get<1>(declmaps[i]) == texture)
    {
      slot = i;

      if (get<1>(declmaps[i]) == texture)
        break;
    }
  }

  if (get<1>(declmaps[slot]) != texture)
  {
    get<1>(declmaps[slot]) = texture;

    bind_texture(context.vulkan, context.framedescriptors[context.frame & 1], ShaderLocation::decalmaps, texture, slot);
  }

  get<0>(declmaps[slot]) = context.frame;

  return slot;
}


///////////////////////// prepare_sceneset //////////////////////////////////
void prepare_sceneset(RenderContext &context, PushBuffer const &renderables, RenderParams const &params)
{
  SceneSet sceneset;

  sceneset.proj = context.proj;
  sceneset.invproj = inverse(sceneset.proj);
  sceneset.view = context.view;
  sceneset.invview = inverse(sceneset.view);
  sceneset.worldview = context.proj * context.view;
  sceneset.prevview = context.prevcamera.view();
  sceneset.skyview = (inverse(params.skyboxorientation) * Transform::rotation(context.camera.rotation())).matrix() * sceneset.invproj;
  sceneset.viewport = Vec4(context.fbox, context.fboy, context.width - 2*context.fbox, context.height - 2*context.fboy);

  sceneset.camera.position = context.camera.position();
  sceneset.camera.exposure = context.camera.exposure();
  sceneset.camera.focalwidth = context.camera.focalwidth();
  sceneset.camera.focaldistance = context.camera.focaldistance();
  sceneset.camera.skyboxlod = params.skyboxlod;
  sceneset.camera.ambientintensity = params.ambientintensity;
  sceneset.camera.specularintensity = params.specularintensity;
  sceneset.camera.ssrstrength = params.ssrstrength;
  sceneset.camera.bloomstrength = params.bloomstrength;
  sceneset.camera.fogdensity = Vec4(params.fogattenuation, params.fogdensity);
  sceneset.camera.frame = context.frame;

  sceneset.mainlight.direction = params.sundirection;
  sceneset.mainlight.intensity = params.sunintensity;

  assert(sizeof(context.shadows.shadowview) <= sizeof(sceneset.mainlight.shadowview));
  memcpy(sceneset.mainlight.splits, context.shadows.splits.data(), sizeof(context.shadows.splits));
  memcpy(sceneset.mainlight.shadowview, context.shadows.shadowview.data(), sizeof(context.shadows.shadowview));

  sceneset.environmentcount = 0;
  sceneset.pointlightcount = 0;
  sceneset.spotlightcount = 0;
  sceneset.probecount = 0;
  sceneset.decalcount = 0;

  VkDescriptorImageInfo envmapinfos[extentof(sceneset.environments)] = {};
  VkDescriptorImageInfo spotmapinfos[extentof(sceneset.spotlights)] = {};

  for(auto &renderable : renderables)
  {
    if (renderable.type == Renderable::Type::Lights)
    {
      auto lights = renderable_cast<Renderable::Lights>(&renderable)->lightlist;

      for(size_t i = 0; lights && i < lights->pointlightcount && sceneset.pointlightcount < extentof(sceneset.pointlights); ++i)
      {
        sceneset.pointlights[sceneset.pointlightcount].position = lights->pointlights[i].position;
        sceneset.pointlights[sceneset.pointlightcount].intensity = lights->pointlights[i].intensity;
        sceneset.pointlights[sceneset.pointlightcount].attenuation = lights->pointlights[i].attenuation;
        sceneset.pointlights[sceneset.pointlightcount].attenuation.w = params.lightfalloff * lights->pointlights[i].attenuation.w;

        ++sceneset.pointlightcount;
      }

      for(size_t i = 0; lights && i < lights->spotlightcount && sceneset.spotlightcount < extentof(sceneset.spotlights); ++i)
      {
        sceneset.spotlights[sceneset.spotlightcount].position = lights->spotlights[i].position;
        sceneset.spotlights[sceneset.spotlightcount].intensity = lights->spotlights[i].intensity;
        sceneset.spotlights[sceneset.spotlightcount].attenuation = lights->spotlights[i].attenuation;
        sceneset.spotlights[sceneset.spotlightcount].attenuation.w = params.lightfalloff * lights->spotlights[i].attenuation.w;
        sceneset.spotlights[sceneset.spotlightcount].direction = lights->spotlights[i].direction;
        sceneset.spotlights[sceneset.spotlightcount].cutoff = lights->spotlights[i].cutoff;
        sceneset.spotlights[sceneset.spotlightcount].shadowview = inverse(lights->spotlights[i].spotview);

        spotmapinfos[sceneset.spotlightcount].sampler = lights->spotlights[i].spotmap->texture.sampler;
        spotmapinfos[sceneset.spotlightcount].imageView = lights->spotlights[i].spotmap->texture.imageview;
        spotmapinfos[sceneset.spotlightcount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        ++sceneset.spotlightcount;
      }

      for(size_t i = 0; lights && i < lights->probecount && sceneset.probecount + 1 < extentof(sceneset.probes); ++i)
      {
        sceneset.probes[sceneset.probecount].position = lights->probes[i].position;
        sceneset.probes[sceneset.probecount].irradiance = lights->probes[i].irradiance;

        ++sceneset.probecount;
      }

      for(size_t i = 0; lights && i < lights->environmentcount && sceneset.environmentcount + 1 < extentof(sceneset.environments); ++i)
      {
        assert(lights->environments[i].envmap && lights->environments[i].envmap->ready());

        envmapinfos[sceneset.environmentcount].sampler = lights->environments[i].envmap->texture.sampler;
        envmapinfos[sceneset.environmentcount].imageView = lights->environments[i].envmap->texture.imageview;
        envmapinfos[sceneset.environmentcount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        sceneset.environments[sceneset.environmentcount].halfdim = lights->environments[i].size/2;
        sceneset.environments[sceneset.environmentcount].invtransform = inverse(lights->environments[i].transform);

        ++sceneset.environmentcount;
      }
    }

    if (renderable.type == Renderable::Type::Decals)
    {
      auto decals = renderable_cast<Renderable::Decals>(&renderable)->decallist;

      for(size_t i = 0; decals && i < decals->decalcount && sceneset.decalcount < extentof(sceneset.decals); ++i)
      {
        assert(decals->decals[i].material);
        assert(decals->decals[i].material->albedomap && decals->decals[i].material->albedomap->ready());
        assert(decals->decals[i].material->normalmap && decals->decals[i].material->normalmap->ready());

        sceneset.decals[sceneset.decalcount].halfdim = decals->decals[i].size/2;
        sceneset.decals[sceneset.decalcount].invtransform = inverse(decals->decals[i].transform);
        sceneset.decals[sceneset.decalcount].color = hada(decals->decals[i].material->color, decals->decals[i].tint);
        sceneset.decals[sceneset.decalcount].metalness = decals->decals[i].material->metalness;
        sceneset.decals[sceneset.decalcount].roughness = decals->decals[i].material->roughness;
        sceneset.decals[sceneset.decalcount].reflectivity = decals->decals[i].material->reflectivity;
        sceneset.decals[sceneset.decalcount].emissive = decals->decals[i].material->emissive;
        sceneset.decals[sceneset.decalcount].albedomap = bind_decalmap(context, decals->decals[i].material->albedomap->texture);
        sceneset.decals[sceneset.decalcount].normalmap = bind_decalmap(context, decals->decals[i].material->normalmap->texture);
        sceneset.decals[sceneset.decalcount].texcoords = decals->decals[i].extent;
        sceneset.decals[sceneset.decalcount].layer = decals->decals[i].layer;
        sceneset.decals[sceneset.decalcount].mask = decals->decals[i].mask;

        ++sceneset.decalcount;
      }
    }
  }

  if (params.skybox)
  {
    assert(params.skybox->ready());

    envmapinfos[sceneset.environmentcount].sampler = params.skybox->texture.sampler;
    envmapinfos[sceneset.environmentcount].imageView = params.skybox->texture.imageview;
    envmapinfos[sceneset.environmentcount].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    sceneset.environments[sceneset.environmentcount].halfdim = Vec3(1e5f, 1e5f, 1e5f);
    sceneset.environments[sceneset.environmentcount].invtransform = inverse(params.skyboxorientation);

    sceneset.environmentcount += 1;
  }

  if (sceneset.spotlightcount != 0)
  {
    bind_texture(context.vulkan, context.framedescriptors[context.frame & 1], ShaderLocation::spotmaps, spotmapinfos, sceneset.spotlightcount);
  }

  if (sceneset.environmentcount != 0)
  {
    bind_texture(context.vulkan, context.framedescriptors[context.frame & 1], ShaderLocation::envmaps, envmapinfos, sceneset.environmentcount);
  }

  if (params.colorlut)
  {
    assert(params.colorlut->ready());

    bind_texture(context.vulkan, context.framedescriptors[context.frame & 1], ShaderLocation::colorlut, params.colorlut->texture);
  }

  update(context.commandbuffers[context.frame & 1], context.sceneset, 0, sizeof(SceneSet), &sceneset);
}


///////////////////////// draw_calls ////////////////////////////////////////
extern void draw_prepass(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Geometry const &geometry);
extern void draw_geometry(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Geometry const &geometry);
extern void draw_forward(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Forward::Command const *commands);
extern void draw_casters(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Casters const &casters);
extern void draw_sprites(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Sprites const &sprites);
extern void draw_overlays(RenderContext &context, VkCommandBuffer commandbuffer, Renderable::Overlays const &overlays);


///////////////////////// render_fallback ///////////////////////////////////
void render_fallback(RenderContext &context, DatumPlatform::Viewport const &viewport, const void *bitmap, int width, int height)
{
  assert(context.vulkan);

  wait_fence(context.vulkan, context.framefence);

  CommandBuffer &commandbuffer = context.commandbuffers[context.frame & 1];

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  setimagelayout(commandbuffer, viewport.image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

  clear(commandbuffer, viewport.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Color4(0.0f, 0.0f, 0.0f, 1.0f));

  if (bitmap)
  {
    size_t size = width * height * sizeof(uint32_t);

    assert(size < TransferBufferSize);

    memcpy(map_memory<uint8_t>(context.vulkan, context.transferbuffer, 0, size), bitmap, size);

    blit(commandbuffer, context.transferbuffer, 0, viewport.image, max(viewport.width - width, 0)/2, max(viewport.height - height, 0)/2, width, height, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 });
  }

  setimagelayout(commandbuffer, viewport.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

  end(context.vulkan, commandbuffer);

  submit(context.vulkan, commandbuffer, viewport.rendercomplete, context.framefence, { viewport.acquirecomplete });

  vkQueueWaitIdle(context.vulkan.queue);

  context.luminance = 1.0f;

  ++context.frame;
}


///////////////////////// render ////////////////////////////////////////////
void render(RenderContext &context, DatumPlatform::Viewport const &viewport, Camera const &camera, PushBuffer const &renderables, RenderParams const &params, VkSemaphore const (&dependancies)[7])
{
  assert(context.ready);

  context.camera = camera;
  context.proj = camera.proj();
  context.view = camera.view();

  auto &commandbuffer = context.commandbuffers[context.frame & 1];

  begin(context.vulkan, commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  reset_querypool(commandbuffer, context.timingquerypool, 0, 1);

  prepare_shadowview(context.shadows, camera, params.sundirection);

  prepare_sceneset(context, renderables, params);

  VkClearValue clearvalues[4];
  clearvalues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
  clearvalues[1].color = { 0.0f, 0.0f, 0.0f, 0.0f };
  clearvalues[2].color = { 0.0f, 0.0f, 0.0f, 0.0f };
  clearvalues[3].depthStencil = { 1, 0 };

  auto &framedescriptor = context.framedescriptors[context.frame & 1];

  bind_descriptor(commandbuffer, context.pipelinelayout, ShaderLocation::sceneset, framedescriptor, VK_PIPELINE_BIND_POINT_COMPUTE);

  querytimestamp(commandbuffer, context.timingquerypool, 0);

  //
  // Shadows
  //

  beginpass(commandbuffer, context.shadowpass, context.shadowbuffer, 0, 0, context.shadows.width, context.shadows.height, 1, &clearvalues[3]);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Casters:
        draw_casters(context, commandbuffer, *renderable_cast<Renderable::Casters>(&renderable));
        break;

      default:
        break;
    }
  }

  endpass(commandbuffer, context.shadowpass);

  if (params.fogdensity != 0)
  {
    // ESM Downsample

    bind_pipeline(commandbuffer, context.esmpipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.esmshadowbuffer, context.esmshadowbuffer.width, context.esmshadowbuffer.height, 1, computeconstants.ESMGenDispatch);

    bind_pipeline(commandbuffer, context.esmpipeline[1], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.scratchbuffers[3], context.esmshadowbuffer.width, context.esmshadowbuffer.height, 1, computeconstants.ESMHBlurDispatch);

    bind_pipeline(commandbuffer, context.esmpipeline[2], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.esmshadowbuffer, context.esmshadowbuffer.width, context.esmshadowbuffer.height, 1, computeconstants.ESMVBlurDispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 1);

  //
  // Prepass
  //

  beginpass(commandbuffer, context.prepass, context.preframebuffer, 0, 0, context.fbowidth, context.fboheight, 1, &clearvalues[3]);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Geometry:
        draw_prepass(context, commandbuffer, *renderable_cast<Renderable::Geometry>(&renderable));
        break;

      default:
        break;
    }
  }

  endpass(commandbuffer, context.prepass);

  bind_pipeline(commandbuffer, context.depthmippipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, context.depthmipbuffer, context.fbowidth/2, context.fboheight/2, 1, computeconstants.DepthMipDispatch);

  querytimestamp(commandbuffer, context.timingquerypool, 2);

  //
  // Geometry
  //

  beginpass(commandbuffer, context.geometrypass, context.geometryframebuffer, 0, 0, context.fbowidth, context.fboheight, 4, &clearvalues[0]);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Geometry:
        draw_geometry(context, commandbuffer, *renderable_cast<Renderable::Geometry>(&renderable));
        break;

      default:
        break;
    }
  }

  endpass(commandbuffer, context.geometrypass);

  querytimestamp(commandbuffer, context.timingquerypool, 3);

  //
  // Cluster
  //

  bind_pipeline(commandbuffer, context.clusterpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, context.fbowidth, context.fboheight, 1, computeconstants.ClusterDispatch);

  querytimestamp(commandbuffer, context.timingquerypool, 4);

  //
  // Lighting
  //

  if (params.ssaoscale != 0)
  {
    // SSAO

    bind_pipeline(commandbuffer, context.ssaopipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.ssaobuffers[context.frame & 1], context.ssaobuffers[context.frame & 1].width, context.ssaobuffers[context.frame & 1].height, 1, computeconstants.SSAODispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 5);

  barrier(commandbuffer, context.sceneset, 0, context.sceneset.size);

  if (params.fogdensity != 0)
  {
    // Volumetric Fog

    bind_pipeline(commandbuffer, context.fogvolumepipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fogvolumebuffers[(context.frame & 1) ^ 1], computeconstants.FogVolumeX, computeconstants.FogVolumeY, computeconstants.FogVolumeZ, computeconstants.FogDensityDispatch);

    bind_pipeline(commandbuffer, context.fogvolumepipeline[1], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.fogvolumebuffers[context.frame & 1], computeconstants.FogVolumeX, computeconstants.FogVolumeY, 1, computeconstants.FogScatterDispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 6);

  bind_pipeline(commandbuffer, context.lightingpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, context.colorbuffer, context.colorbuffer.width, context.colorbuffer.height, 1, computeconstants.LightingDispatch);

  querytimestamp(commandbuffer, context.timingquerypool, 7);

  //
  // Forward
  //

  auto &forwardcommands = context.forwardcommands[context.frame & 1];

  bool solids = any_of(renderables.begin(), renderables.end(), [](auto &renderable) { return (renderable.type == Renderable::Type::Forward && renderable_cast<Renderable::Forward>(&renderable)->solidcommands); });
  bool blends = any_of(renderables.begin(), renderables.end(), [](auto &renderable) { return (renderable.type == Renderable::Type::Forward && renderable_cast<Renderable::Forward>(&renderable)->blendcommands); });

  beginpass(commandbuffer, context.forwardpass, context.forwardframebuffer, 0, 0, context.fbowidth, context.fboheight, 0, nullptr);

  // Solids

  if (solids)
  {
    begin(context.vulkan, forwardcommands[0], context.forwardframebuffer, context.forwardpass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

    bind_descriptor(forwardcommands[0], context.pipelinelayout, ShaderLocation::sceneset, framedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);

    for(auto &renderable : renderables)
    {
      if (renderable.type == Renderable::Type::Forward && renderable_cast<Renderable::Forward>(&renderable)->solidcommands)
      {
        draw_forward(context, forwardcommands[0], renderable_cast<Renderable::Forward>(&renderable)->solidcommands);
      }
    }

    end(context.vulkan, forwardcommands[0]);

    execute(commandbuffer, forwardcommands[0]);
  }

  nextsubpass(commandbuffer, context.forwardpass);

  // Blend

  if (blends)
  {
    begin(context.vulkan, forwardcommands[1], context.forwardframebuffer, context.forwardpass, 1, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

    bind_descriptor(forwardcommands[1], context.pipelinelayout, ShaderLocation::sceneset, framedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);

    clear(forwardcommands[1], 0, 0, context.fbowidth, context.fboheight, 0, Color4(0.0f, 0.0f, 0.0f, 1.0f));
    clear(forwardcommands[1], 0, 0, context.fbowidth, context.fboheight, 1, Color4(0.0f, 0.0f, 0.0f, 0.0f));

    for(auto &renderable : renderables)
    {
      if (renderable.type == Renderable::Type::Forward && renderable_cast<Renderable::Forward>(&renderable)->blendcommands)
      {
        draw_forward(context, forwardcommands[1], renderable_cast<Renderable::Forward>(&renderable)->blendcommands);
      }
    }

    end(context.vulkan, forwardcommands[1]);

    execute(commandbuffer, forwardcommands[1]);
  }

  nextsubpass(commandbuffer, context.forwardpass);

  // Color

  begin(context.vulkan, forwardcommands[2], context.forwardframebuffer, context.forwardpass, 2, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

  bind_descriptor(forwardcommands[2], context.pipelinelayout, ShaderLocation::sceneset, framedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);

  if (params.skybox)
  {
    bind_pipeline(forwardcommands[2], context.skyboxpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_vertexbuffer(forwardcommands[2], 0, context.unitquad);
    draw(forwardcommands[2], context.unitquad.vertexcount, 1, 0, 0);
  }

  for(auto &renderable : renderables)
  {
    if (renderable.type == Renderable::Type::Forward && renderable_cast<Renderable::Forward>(&renderable)->colorcommands)
    {
      draw_forward(context, forwardcommands[2], renderable_cast<Renderable::Forward>(&renderable)->colorcommands);
    }
  }

  if (blends)
  {
    bind_pipeline(forwardcommands[2], context.weightblendpipeline, 0, 0, context.fbowidth, context.fboheight, VK_PIPELINE_BIND_POINT_GRAPHICS);
    bind_vertexbuffer(forwardcommands[2], 0, context.unitquad);
    draw(forwardcommands[2], context.unitquad.vertexcount, 1, 0, 0);
  }

  end(context.vulkan, forwardcommands[2]);

  execute(commandbuffer, forwardcommands[2]);

  endpass(commandbuffer, context.forwardpass);

  querytimestamp(commandbuffer, context.timingquerypool, 8);

  //
  // Color Blur
  //

  if (computeconstants.DepthOfField)
  {
    bind_pipeline(commandbuffer, context.colorblurpipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.scratchbuffers[2], context.colorbuffer.width/2, context.colorbuffer.height, 1, computeconstants.ColorHBlurDispatch);

    bind_pipeline(commandbuffer, context.colorblurpipeline[1], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.colorbuffer, context.colorbuffer.width/2, context.colorbuffer.height/2, 1, computeconstants.ColorVBlurDispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 9);

  //
  // SSR
  //

  if (params.ssrstrength != 0)
  {
    for(uint32_t i = 0; i < 6; ++i)
    {
      bind_pipeline(commandbuffer, context.depthmippipeline[i], VK_PIPELINE_BIND_POINT_COMPUTE);

      dispatch(commandbuffer, context.depthmipbuffer, context.depthmipbuffer.width >> i, context.depthmipbuffer.height >> i, 1, computeconstants.DepthMipDispatch);
    }

    bind_pipeline(commandbuffer, context.ssrpipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.scratchbuffers[2], context.scratchbuffers[2].width, context.scratchbuffers[2].height, 1, computeconstants.SSRDispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 10);

  //
  // Luminance
  //

  bind_pipeline(commandbuffer, context.luminancepipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

  dispatch(commandbuffer, 1, 1, 1);

  querytimestamp(commandbuffer, context.timingquerypool, 11);

  //
  // Bloom
  //

  if (params.bloomstrength != 0)
  {
    bind_pipeline(commandbuffer, context.bloompipeline[0], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.scratchbuffers[0], context.scratchbuffers[0].width, context.scratchbuffers[0].height, 1, computeconstants.BloomLumaDispatch);

    bind_pipeline(commandbuffer, context.bloompipeline[1], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.scratchbuffers[1], context.scratchbuffers[1].width, context.scratchbuffers[1].height, 1, computeconstants.BloomHBlurDispatch);

    bind_pipeline(commandbuffer, context.bloompipeline[2], VK_PIPELINE_BIND_POINT_COMPUTE);

    dispatch(commandbuffer, context.scratchbuffers[0], context.scratchbuffers[0].width, context.scratchbuffers[0].height, 1, computeconstants.BloomVBlurDispatch);
  }

  querytimestamp(commandbuffer, context.timingquerypool, 12);

  //
  // Overlay
  //

  auto &compositecommands = context.compositecommands[context.frame & 1];
  begin(context.vulkan, compositecommands, context.framebuffer, context.overlaypass, 0, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
  bind_pipeline(compositecommands, context.compositepipeline, context.fbox, context.fboy, context.width-2*context.fbox, context.height-2*context.fboy, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_descriptor(compositecommands, context.pipelinelayout, ShaderLocation::sceneset, framedescriptor, VK_PIPELINE_BIND_POINT_GRAPHICS);
  bind_vertexbuffer(compositecommands, 0, context.unitquad);
  draw(compositecommands, context.unitquad.vertexcount, 1, 0, 0);
  end(context.vulkan, compositecommands);

  beginpass(commandbuffer, context.overlaypass, context.framebuffer, 0, 0, context.width, context.height, 2, &clearvalues[2]);

  execute(commandbuffer, compositecommands);

  for(auto &renderable : renderables)
  {
    switch (renderable.type)
    {
      case Renderable::Type::Sprites:
        draw_sprites(context, commandbuffer, *renderable_cast<Renderable::Sprites>(&renderable));
        break;

      case Renderable::Type::Overlays:
        draw_overlays(context, commandbuffer, *renderable_cast<Renderable::Overlays>(&renderable));
        break;

      default:
        break;
    }
  }

  endpass(commandbuffer, context.overlaypass);

  querytimestamp(commandbuffer, context.timingquerypool, 13);

  //
  // Blit
  //

  if (viewport.image)
  {
    setimagelayout(commandbuffer, viewport.image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    blit(commandbuffer, context.rendertarget.image, 0, 0, context.rendertarget.width, context.rendertarget.height, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, viewport.image, viewport.x, viewport.y, viewport.width, viewport.height, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, VK_FILTER_LINEAR);

    setimagelayout(commandbuffer, viewport.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
  }

  querytimestamp(commandbuffer, context.timingquerypool, 14);

  barrier(commandbuffer);

  end(context.vulkan, commandbuffer);

  //
  // Submit
  //

  BEGIN_TIMED_BLOCK(Wait, Color3(0.1f, 0.1f, 0.1f))

  wait_fence(context.vulkan, context.framefence);

  END_TIMED_BLOCK(Wait)

  // Feedback

  context.luminance = map_memory<LumaSet>(context.vulkan, context.transferbuffer, 0, sizeof(LumaSet))->luminance;

  // Timing Queries

  uint64_t timings[16];
  retreive_querypool(context.vulkan, context.timingquerypool, 0, 15, timings);

  GPU_TIMED_BLOCK(Shadows, Color3(0.0f, 0.4f, 0.0f), timings[0], timings[1])
  GPU_TIMED_BLOCK(PrePass, Color3(0.4f, 0.2f, 0.4f), timings[1], timings[2])
  GPU_TIMED_BLOCK(Geometry, Color3(0.4f, 0.0f, 0.4f), timings[2], timings[3])
  GPU_TIMED_BLOCK(Cluster, Color3(0.5f, 0.5f, 0.1f), timings[3], timings[4])
  GPU_TIMED_BLOCK(SSAO, Color3(0.2f, 0.8f, 0.2f), timings[4], timings[5])
  GPU_TIMED_BLOCK(Fog, Color3(0.0f, 0.2f, 0.2f), timings[5], timings[6])
  GPU_TIMED_BLOCK(Lighting, Color3(0.0f, 0.6f, 0.4f), timings[6], timings[7])
  GPU_TIMED_BLOCK(Forward, Color3(0.2f, 0.3f, 0.6f), timings[7], timings[8])
  GPU_TIMED_BLOCK(Blur, Color3(0.2f, 0.5f, 0.2f), timings[8], timings[9])
  GPU_TIMED_BLOCK(SSR, Color3(0.0f, 0.4f, 0.8f), timings[9], timings[10])
  GPU_TIMED_BLOCK(Luminance, Color3(0.8f, 0.4f, 0.2f), timings[10], timings[11])
  GPU_TIMED_BLOCK(Bloom, Color3(0.5f, 0.2f, 0.6f), timings[11], timings[12])
  GPU_TIMED_BLOCK(Overlay, Color3(0.4f, 0.4f, 0.0f), timings[12], timings[13])
  GPU_TIMED_BLOCK(Blit, Color3(0.4f, 0.4f, 0.4f), timings[13], timings[14])

  GPU_SUBMIT();

  submit(context.vulkan, commandbuffer, viewport.rendercomplete, context.framefence, { viewport.acquirecomplete, dependancies[0], dependancies[1], dependancies[2], dependancies[3], dependancies[4], dependancies[5], dependancies[6] });

  context.prevcamera = camera;

  ++context.frame;
}
