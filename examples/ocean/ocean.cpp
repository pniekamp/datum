//
// ocean.cpp
//

#include "ocean.h"
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
  initialise_ocean_context(platform, state.oceancontext, 0);

  state.camera.set_projection(state.fov*pi<float>()/180.0f, state.aspect);

  auto core = state.assets.load(platform, "core.pack");

  if (!core)
    throw runtime_error("Core Assets Load Failure");

  if (core->magic != CoreAsset::magic || core->version != CoreAsset::version)
    throw runtime_error("Core Assets Version Mismatch");

  state.debugfont = state.resources.create<Font>(state.assets.find(CoreAsset::debug_font));
  state.defaultmaterial = state.resources.create<Material>(state.assets.find(CoreAsset::default_material));

  state.ocean.wavescale = 22.0f;
  state.ocean.waveamplitude = 0.0025f;
  state.ocean.swellamplitude = 0.8f;
  state.ocean.windspeed = 7.9f;
  state.ocean.smoothing = 340.0f;

  seed_ocean(state.ocean);

  auto watercolor = state.resources.create<Texture>(state.assets.find(CoreAsset::wave_color), Texture::Format::RGBE);
  auto watersurface = state.resources.create<Texture>(state.assets.find(CoreAsset::wave_foam), Texture::Format::RGBA);
  auto waternormal = state.resources.create<Texture>(state.assets.find(CoreAsset::wave_normal), Texture::Format::RGBA);
  state.oceanmaterial = state.resources.create<Material>(Color4(0.468f, 0.686f, 0.74f, 1), 0.0f, 0.32f, 0.02f, 0.0f, watercolor, watersurface, waternormal);

  state.oceanmesh = make_plane(state.resources, 1024, 1024);

  state.skybox = state.resources.create<SkyBox>(state.assets.find(CoreAsset::default_skybox));

  state.camera.lookat(Vec3(0, 0, 8), Vec3(1, 0, 8), Vec3(0, 0, 1));

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

    if (state.rendercontext.ready && state.oceancontext.ready && state.debugfont->ready() && state.defaultmaterial->ready())
    {
      state.mode = GameState::Load;
    }
  }

  if (state.mode == GameState::Load)
  {
    asset_guard lock(state.assets);

    int ready = 0, total = 0;

    request(platform, state.resources, state.oceanmaterial, &ready, &total);
    request(platform, state.resources, state.skybox, &ready, &total);

    if (ready == total)
    {
      state.mode = GameState::Play;
    }
  }

  if (state.mode == GameState::Play)
  {
    state.time += dt;

    if (input.mousebuttons[GameInput::Left].down())
    {
      state.camera.yaw(-1.5f * input.deltamousex, Vec3(0, 0, 1));
      state.camera.pitch(-1.5f * input.deltamousey);
    }

    state.camera = adapt(state.camera, state.rendercontext.luminance, 0.25f, 0.5f*dt);

    state.camera = normalise(state.camera);

    update_ocean(state.ocean, dt);
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
  renderparams.skybox = state.skybox;
  renderparams.sundirection = Vec3(-0.57735f, -0.57735f, -0.57735f);
  renderparams.skyboxorientation = Transform::rotation(Vec3(1, 0, 0), pi<float>()/2);
  renderparams.ssaoscale = 0.0f;
  renderparams.fogdensity = 0.0f;
  renderparams.fogattenuation = Vec3(0.0f, 0.0f, 0.5f);

  if (state.mode == GameState::Startup)
  {
    if (prepare_render_context(platform, state.rendercontext, state.assets))
    {
      prepare_render_pipeline(state.rendercontext, renderparams);
    }

    prepare_ocean_context(platform, state.oceancontext, state.assets);

    render_fallback(state.rendercontext, viewport);
  }

  if (state.mode == GameState::Load)
  {
    render_fallback(state.rendercontext, viewport);
  }

  if (state.mode == GameState::Play)
  {
    auto &camera = state.camera;

    render_ocean_surface(state.oceancontext, state.oceanmesh, 1024, 1024, camera, state.ocean);

    RenderList renderlist(platform.renderscratchmemory, 8*1024*1024);

    {
      GeometryList geometry;
      GeometryList::BuildState buildstate;

      if (geometry.begin(buildstate, state.rendercontext, state.resources))
      {
        Vec3 bumpscale = Vec3(0.2f, 0.2f, 0.2f);
        float foamwaveheight = 0.55f;
        float foamwavescale = 0.2f;
        float foamshoreheight = 0.1f;
        float foamshorescale = 0.02f;

        geometry.push_ocean(buildstate, Transform::identity(), state.oceanmesh, state.oceanmaterial, 0.0004f*state.ocean.flow, bumpscale, state.ocean.plane, foamwaveheight, foamwavescale, foamshoreheight, foamshorescale);

        geometry.finalise(buildstate);
      }

      renderlist.push_geometry(geometry);
    }

    render(state.rendercontext, viewport, camera, renderlist, renderparams, { state.oceancontext.rendercomplete });
  }

  state.resources.release(state.resourcetoken);
}
