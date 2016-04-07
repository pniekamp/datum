//
// datumtest.h
//

#pragma once

#include "datum/platform.h"
#include "datum/asset.h"
#include "datum/math.h"
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

  float lastmousex, lastmousey, lastmousez;

  AssetManager assets;

  ResourceManager resources;

  RenderContext rendercontext;

  // Render Frames

  struct RenderFrame
  {
    float time;

    Camera camera;

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
