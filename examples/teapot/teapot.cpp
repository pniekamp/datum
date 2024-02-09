//
// teapot.cpp
//

#include "teapot.h"
#include "datum/debug.h"

using namespace std;
using namespace lml;
using namespace DatumPlatform;


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

  state.camera.set_projection(state.fov*pi<float>()/180.0f, state.aspect);

  auto core = state.assets.load(platform, "core.pack");

  if (!core)
    throw runtime_error("Core Assets Load Failure");

  if (core->magic != CoreAsset::magic || core->version != CoreAsset::version)
    throw runtime_error("Core Assets Version Mismatch");

  state.debugfont = state.resources.create<Font>(state.assets.find(CoreAsset::debug_font));
  state.defaultmaterial = state.resources.create<Material>(state.assets.find(CoreAsset::default_material));

  state.mesh = state.resources.create<Mesh>(state.assets.load(platform, "teapot.pack"));
  state.material = state.resources.create<Material>(Color4(1.0f, 0.0f, 0.0f, 1.0f), 0.0f, 0.3f);

  state.mode = GameState::Startup;
}


///////////////////////// game_resize ///////////////////////////////////////
void example_resize(PlatformInterface &platform, Viewport const &viewport)
{
  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  if (state.rendercontext.ready)
  {
    RenderParams renderparams;
    renderparams.width = viewport.width;
    renderparams.height = viewport.height;
    renderparams.aspect = state.aspect;
    renderparams.fogdensity = 0.0f;

    prepare_render_pipeline(state.rendercontext, renderparams);
  }
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

    if (ready == total)
    {
      state.mode = GameState::Play;
    }
  }

  if (state.mode == GameState::Play)
  {
    state.time += dt;
  }
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

  state.resourcetoken = state.resources.token();

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
    render_fallback(state.rendercontext, viewport);
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
        geometry.push_mesh(buildstate, Transform::translation(0.0f, 0.0f, -5.0f) * Transform::rotation(Vec3(0, 1, 0), state.time), state.mesh, state.material);

        geometry.finalise(buildstate);
      }

      renderlist.push_geometry(geometry);
    }

    render(state.rendercontext, viewport, camera, renderlist, renderparams);
  }

  state.resources.release(state.resourcetoken);
}
