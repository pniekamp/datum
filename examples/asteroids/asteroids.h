//
// asteroids.h
//

#pragma once

#include "datum.h"
#include "datum/renderer.h"

static const int LodLevels = 4;
static const int RoidCount = 50000;
static const int MeshCount = 1000;
static const int ThreadCount = 4;

static const float SimOrbitRadius = 450.0f;
static const float SimDiscRadius = 120.0f;

struct Asteroid
{
  float mass;

  Mesh const *lods[LodLevels];
};

struct Instance
{
  lml::Transform transform;

  lml::Vec3 spinaxis;
  float spinvelocity;

  float orbitvelocity;

  int mesh;
  int material;
};

//|---------------------- GameState -----------------------------------------
//|--------------------------------------------------------------------------

struct GameState
{
  GameState(StackAllocator<> const &allocator);

  const float fov = 60.0f;
  const float aspect = 1920.0f/1080.0f;

  enum { Startup, Init, Play } mode;

  float time = 0;

  Camera camera;

  Font const *debugfont;
  Material const *defaultmaterial;

  AssetManager assets;

  ResourceManager resources;

  RenderContext rendercontext;

  SkyBox const *skybox;

  Material const *materials[5];

  Asteroid meshes[MeshCount];
  Instance instances[RoidCount];

  GeometryList cmdlists[ThreadCount];

  std::atomic<bool> initialised;

  size_t resourcetoken = 0;
};

void example_init(DatumPlatform::PlatformInterface &platform);
void example_resize(DatumPlatform::PlatformInterface &platform, DatumPlatform::Viewport const &viewport);
void example_update(DatumPlatform::PlatformInterface &platform, DatumPlatform::GameInput const &input, float dt);
void example_render(DatumPlatform::PlatformInterface &platform, DatumPlatform::Viewport const &viewport);
