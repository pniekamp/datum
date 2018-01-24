//
// teapot.h
//

#pragma once

#include "datum.h"
#include "datum/renderer.h"

//|---------------------- GameState -----------------------------------------
//|--------------------------------------------------------------------------

struct GameState
{
  GameState(StackAllocator<> const &allocator);

  const float fov = 60.0f;
  const float aspect = 1920.0f/1080.0f;

  enum { Startup, Load, Play } mode;

  float time = 0;

  Camera camera;

  Font const *debugfont;
  Material const *defaultmaterial;

  AssetManager assets;

  ResourceManager resources;

  RenderContext rendercontext;

  Mesh const *mesh;
  Material const *material;

  size_t resourcetoken = 0;
};

void example_init(DatumPlatform::PlatformInterface &platform);
void example_update(DatumPlatform::PlatformInterface &platform, DatumPlatform::GameInput const &input, float dt);
void example_render(DatumPlatform::PlatformInterface &platform, DatumPlatform::Viewport const &viewport);
