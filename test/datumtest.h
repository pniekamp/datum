//
// datumtest.h
//

#pragma once

#include "datum.h"
#include "datum/asset.h"
#include "datum/math.h"
#include "datum/scene.h"
#include "datum/renderer.h"

//|---------------------- GameState -----------------------------------------
//|--------------------------------------------------------------------------

struct GameState
{
  using Vec3 = lml::Vec3;
  using Color3 = lml::Color3;
  using Transform = lml::Transform;
  using Attenuation = lml::Attenuation;

  GameState(StackAllocator<> const &allocator);

  const float fov = 60.0f;
  const float aspect = 1920.0f/1080.0f;

  enum { Startup, Init, Load, Play } mode;

  float time = 0;

  Camera camera;

  float luminancetarget = 0.15f;

  Sprite const *loader;
  Font const *debugfont;
  Mesh const *unitquad;
  Mesh const *unitcone;
  Mesh const *unithemi;
  Mesh const *unitsphere;
  Mesh const *linequad;
  Mesh const *linecube;
  Mesh const *linecone;
  ColorLut const *colorlut;
  Material const *defaultmaterial;

  AssetManager assets;

  ResourceManager resources;

  RenderContext rendercontext;
  SpotMapContext spotmapcontext;

  Scene scene;

  Mesh const *testplane;
  Mesh const *testsphere;
  Mesh const *testcube;
  Sprite const *testimage;

  Transform testspotview;
  SpotMap const *testspotcaster;

  Material const *oceanmaterial;

  Mesh const *suzanne;
  unique_resource<Material> suzannematerial;
  unique_resource<Material> floormaterial;

  ParticleSystem const *testparticlesystem;
  ParticleSystem::Instance *testparticles;

  Mesh const *testactor;
  Animation const *testanimation;

  SkyBox const *skybox;
  Vec3 sundirection = normalise(Vec3(0.4f, -1.0f, -0.1f));
  Color3 sunintensity = Color3(8.0f, 8.0f, 8.0f);

  // Render Frames

  struct RenderFrame
  {
    int mode;

    float time;

    Camera camera;

    SkyBox const *skybox;
    Vec3 sundirection;
    Color3 sunintensity;

    CasterList casters;
    GeometryList geometry;
    LightList lights;
    SpriteList sprites;

    size_t resourcetoken;

  } renderframes[3];

  RenderFrame *readframe;
  RenderFrame *writeframe;
  std::atomic<RenderFrame*> readyframe;
};


void datumtest_init(DatumPlatform::PlatformInterface &platform);
void datumtest_update(DatumPlatform::PlatformInterface &platform, DatumPlatform::GameInput const &input, float dt);
void datumtest_render(DatumPlatform::PlatformInterface &platform, DatumPlatform::Viewport const &viewport);
