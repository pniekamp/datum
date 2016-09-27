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

  float time = 0;

  Camera camera;

  float luminancetarget = 0.15f;

  float lastmousex, lastmousey, lastmousez;

  Sprite const *loader;
  Font const *debugfont;
  Mesh const *unitsphere;
  Material const *defaultmaterial;
  SkyBox const *skybox;

  AssetManager assets;

  ResourceManager resources;

  RenderContext rendercontext;

  Scene scene;

  Mesh const *testplane;
  Mesh const *testsphere;
  Sprite const *testimage;

  Mesh const *suzanne;

  Vec3 sundirection = normalise(Vec3(0.4f, -1.0f, -0.1f));
  Color3 sunintensity = Color3(8.0f, 7.56f, 7.88f);

  SkyboxContext skyboxcontext;

  // Render Frames

  struct RenderFrame
  {
    float time;

    Camera camera;

    MeshList meshes;
    LightList lights;
    SpriteList sprites;
    CasterList casters;

    SkyBox const *skybox;

    size_t resourcetoken;

  } renderframes[3];

  RenderFrame *readframe;
  RenderFrame *writeframe;
  std::atomic<RenderFrame*> readyframe;
};


void datumtest_init(DatumPlatform::PlatformInterface &platform);
void datumtest_update(DatumPlatform::PlatformInterface &platform, DatumPlatform::GameInput const &input, float dt);
void datumtest_render(DatumPlatform::PlatformInterface &platform, DatumPlatform::Viewport const &viewport);
