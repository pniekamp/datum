//
// material.cpp
//

#include "material.h"
#include "datum/debug.h"

using namespace std;
using namespace lml;
using namespace DatumPlatform;
using leap::extentof;

///////////////////////// GameState::Constructor ////////////////////////////
GameState::GameState(StackAllocator<> const &allocator)
  : assets(allocator),
    resources(assets, allocator)
{  
}


///////////////////////// game_init /////////////////////////////////////////
void example_init(PlatformInterface &platform)
{
  GameState &state = *new(allocate<GameState>(platform.gamememory)) GameState(platform.gamememory);

  initialise_asset_system(platform, state.assets, 64*1024, 128*1024*1024);

  initialise_resource_system(platform, state.resources, 2*1024*1024, 8*1024*1024, 64*1024*1024, 1);

  initialise_render_context(platform, state.rendercontext, 16*1024*1024, 0);

  config_render_pipeline(RenderPipelineConfig::EnableDepthOfField, true);

  state.camera.set_projection(state.fov*pi<float>()/180.0f, state.aspect);
  state.camera.set_depthoffield(20000.0f, 85.0f);

  auto core = state.assets.load(platform, "core.pack");

  if (!core)
    throw runtime_error("Core Assets Load Failure");

  if (core->magic != CoreAsset::magic || core->version != CoreAsset::version)
    throw runtime_error("Core Assets Version Mismatch");

  state.debugfont = state.resources.create<Font>(state.assets.find(CoreAsset::debug_font));
  state.defaultmaterial = state.resources.create<Material>(state.assets.find(CoreAsset::default_material));

  auto pack = state.assets.load(platform, "material.pack");

  if (!pack)
    throw runtime_error("Data Assets Load Failure");

  state.mesh = state.resources.create<Mesh>(state.assets.find(pack->id + 1));
  state.material = state.resources.create<Material>(state.assets.find(pack->id + 2));
  state.skyboxes[0] = state.resources.create<SkyBox>(state.assets.find(pack->id + 3));
  state.skyboxes[1] = state.resources.create<SkyBox>(state.assets.find(pack->id + 4));

  state.activeskybox = 0;

  state.mode = GameState::Startup;
}


///////////////////////// game_update ///////////////////////////////////////
void example_update(DatumPlatform::PlatformInterface &platform, DatumPlatform::GameInput const &input, float dt)
{
  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  if (state.mode == GameState::Startup)
  {
    asset_guard lock(state.assets);

    state.resources.request(platform, state.debugfont);
    state.resources.request(platform, state.defaultmaterial);

    if (state.rendercontext.ready && state.debugfont->ready() && state.defaultmaterial->ready())
    {
      state.mode = GameState::Load;
    }
  }

  if (state.mode == GameState::Load)
  {
    asset_guard lock(state.assets);

    int ready = 0, total = 0;

    request(platform, state.resources, state.mesh, &ready, &total);
    request(platform, state.resources, state.material, &ready, &total);
    request(platform, state.resources, state.skyboxes[0], &ready, &total);
    request(platform, state.resources, state.skyboxes[1], &ready, &total);

    if (ready == total)
    {
      state.mode = GameState::Play;
    }
  }

  if (state.mode == GameState::Play)
  {
    state.time += dt;

    if (input.keys[' '].pressed())
      state.activeskybox = (state.activeskybox + 1) % extentof(state.skyboxes);
  }

  state.resourcetoken = state.resources.token();
}


///////////////////////// game_render ///////////////////////////////////////
void example_render(DatumPlatform::PlatformInterface &platform, DatumPlatform::Viewport const &viewport)
{
  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  RenderParams renderparams;
  renderparams.width = viewport.width;
  renderparams.height = viewport.height;
  renderparams.aspect = state.aspect;
  renderparams.fogdensity = 0.0f;

  if (state.mode == GameState::Startup)
  {
    if (prepare_render_context(platform, state.rendercontext, state.assets))
    {
      prepare_render_pipeline(state.rendercontext, renderparams);
    }

    render_fallback(state.rendercontext, viewport);
  }

  if (state.mode == GameState::Load)
  {
    RenderList renderlist(platform.renderscratchmemory, 8*1024*1024);

    SpriteList sprites;
    SpriteList::BuildState buildstate;

    if (sprites.begin(buildstate, state.rendercontext, state.resources))
    {
      sprites.push_text(buildstate, Vec2(viewport.width/2 - state.debugfont->width("Loading...")/2, viewport.height/2 + state.debugfont->height()/2), state.debugfont->height(), state.debugfont, "Loading...");

      sprites.finalise(buildstate);
    }

    renderlist.push_sprites(sprites);

    render(state.rendercontext, viewport, Camera(), renderlist, renderparams);
  }

  if (state.mode == GameState::Play)
  {
    auto &camera = state.camera;

    RenderList renderlist(platform.renderscratchmemory, 8*1024*1024);

    {
      GeometryList geometry;
      GeometryList::BuildState buildstate;

      if (geometry.begin(buildstate, state.rendercontext, state.resources))
      {
        geometry.push_mesh(buildstate, Transform::translation(0.0f, -25.0f, -85.0f) * Transform::rotation(Vec3(0, -1, 0), state.time), state.mesh, state.material);

        geometry.finalise(buildstate);
      }

      renderlist.push_geometry(geometry);
    }

    {
      SpriteList sprites;
      SpriteList::BuildState buildstate;

      if (sprites.begin(buildstate, state.rendercontext, state.resources))
      {
        sprites.push_text(buildstate, Vec2(2.0f, viewport.height - state.debugfont->descent - 2.0f), state.debugfont->height(), state.debugfont, "Spacebar to toggle environment.");

        sprites.finalise(buildstate);
      }

      renderlist.push_sprites(sprites);
    }

    renderparams.skybox = state.skyboxes[state.activeskybox];
    renderparams.skyboxorientation = Transform::rotation(Vec3(0, 1, 0), 1.7f);

    render(state.rendercontext, viewport, camera, renderlist, renderparams);
  }

  state.resources.release(state.resourcetoken);
}
