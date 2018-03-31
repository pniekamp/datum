//
// character.cpp
//

#include "character.h"
#include "datum/debug.h"

using namespace std;
using namespace lml;
using namespace DatumPlatform;


///////////////////////// GameState::Constructor ////////////////////////////
GameState::GameState(StackAllocator<> const &allocator)
  : assets(allocator),
    resources(assets, allocator),
    animator({ allocator, animationfreelist })
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

  auto pack = state.assets.load(platform, "character.pack");

  if (!pack)
    throw runtime_error("Data Assets Load Failure");

  state.mesh = state.resources.create<Mesh>(state.assets.find(pack->id + 1));
  state.material = state.resources.create<Material>(Color4(0.2f, 0.4f, 0.8f, 1.0f), 0.0f, 0.3f);

  state.idle = state.resources.create<Animation>(state.assets.find(pack->id + 2));
  state.walk = state.resources.create<Animation>(state.assets.find(pack->id + 3));
  state.run = state.resources.create<Animation>(state.assets.find(pack->id + 4));

  state.animator.set_mesh(state.mesh);
  state.animator.play(state.idle);
  state.animator.play(state.walk);
  state.animator.play(state.run);

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
    request(platform, state.resources, state.idle, &ready, &total);
    request(platform, state.resources, state.walk, &ready, &total);
    request(platform, state.resources, state.run, &ready, &total);

    if (ready == total)
    {
      state.animator.prepare();

      state.mode = GameState::Play;
    }
  }

  if (state.mode == GameState::Play)
  {
    state.time += dt;

    float speed = 1 + sin(0.1f*state.time);

    float idlewalk = min(speed, 1.0f);
    float walkrun = max(0.0f, speed - 1.0f);

    float rate = idlewalk * lerp(1.0f / state.walk->duration, 1.0f / state.run->duration, walkrun);

    state.animator.set_rate(1, rate * state.walk->duration);
    state.animator.set_rate(2, rate * state.run->duration);
    state.animator.set_weight(0, 1 - idlewalk);
    state.animator.set_weight(1, idlewalk - idlewalk * walkrun);
    state.animator.set_weight(2, idlewalk * walkrun);

    state.animator.update(dt);
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
        geometry.push_mesh(buildstate, Transform::translation(0.0f, -3.0f, -8.0f)*Transform::rotation(Vec3(0, 1, 0), pi<float>()/4), state.animator.pose, state.mesh, state.material);

        geometry.finalise(buildstate);
      }

      renderlist.push_geometry(geometry);
    }

    render(state.rendercontext, viewport, camera, renderlist, renderparams);
  }

  state.resources.release(state.resourcetoken);
}
