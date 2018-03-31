//
// skybox.cpp
//

#include "skybox.h"
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
  initialise_skybox_context(platform, state.skyboxcontext, 0);

  state.camera.set_projection(state.fov*pi<float>()/180.0f, state.aspect);

  auto core = state.assets.load(platform, "core.pack");

  if (!core)
    throw runtime_error("Core Assets Load Failure");

  if (core->magic != CoreAsset::magic || core->version != CoreAsset::version)
    throw runtime_error("Core Assets Version Mismatch");

  state.debugfont = state.resources.create<Font>(state.assets.find(CoreAsset::debug_font));
  state.defaultmaterial = state.resources.create<Material>(state.assets.find(CoreAsset::default_material));

  state.sundirection = Vec3(0.2869f, -0.7174f, -0.6347f);
  state.sunintensity = Color3(8.0f, 7.65f, 6.71f);

  auto clouddensity = state.resources.create<Texture>(state.assets.find(CoreAsset::cloud_density), Texture::Format::RGBA);
  auto cloudnormal = state.resources.create<Texture>(state.assets.find(CoreAsset::cloud_normal), Texture::Format::RGBA);
  state.cloudmaterial = state.resources.create<Material>(Color4(0.3f, 0.3f, 0.3f, 0.7f), 0.0f, 0.0f, 0.0f, 0.0f, clouddensity, cloudnormal);

  state.skybox = state.resources.create<SkyBox>(512, 512, EnvMap::Format::FLOAT16);

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

    if (state.rendercontext.ready && state.skyboxcontext.ready && state.debugfont->ready() && state.defaultmaterial->ready())
    {
      state.mode = GameState::Load;
    }
  }

  if (state.mode == GameState::Load)
  {
    asset_guard lock(state.assets);

    int ready = 0, total = 0;

    request(platform, state.resources, state.cloudmaterial, &ready, &total);
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
      state.camera.yaw(1.5f * (state.lastmousex - input.mousex), Vec3(0, 1, 0));
      state.camera.pitch(1.5f * (state.lastmousey - input.mousey));
    }

    state.lastmousex = input.mousex;
    state.lastmousey = input.mousey;
    state.lastmousez = input.mousez;

    state.sundirection = Transform::rotation(Vec3(-1, 0, 0), 0.1f*dt) * state.sundirection;

    state.camera = adapt(state.camera, state.rendercontext.luminance, 0.25f, 0.5f*dt);

    state.camera = normalise(state.camera);
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
  renderparams.fogdensity = 0.0f;

  if (state.mode == GameState::Startup)
  {
    if (prepare_render_context(platform, state.rendercontext, state.assets))
    {
      prepare_render_pipeline(state.rendercontext, renderparams);
    }

    prepare_skybox_context(platform, state.skyboxcontext, state.assets);

    render_fallback(state.rendercontext, viewport);
  }

  if (state.mode == GameState::Load)
  {
    render_fallback(state.rendercontext, viewport);
  }

  if (state.mode == GameState::Play)
  {
    auto &camera = state.camera;

    SkyBoxParams skyboxparams;
    skyboxparams.skycolor = Color3(1.000f, 0.808f, 0.657f);
    skyboxparams.groundcolor = Color3(0.043f, 0.087f, 0.160f);
    skyboxparams.sunintensity = state.sunintensity;
    skyboxparams.sundirection = state.sundirection;
    skyboxparams.clouds = state.cloudmaterial;
    //skyboxparams.convolesamples = 16;

    render_skybox(state.skyboxcontext, state.skybox, skyboxparams);

    RenderList renderlist(platform.renderscratchmemory, 8*1024*1024);

    render(state.rendercontext, viewport, camera, renderlist, renderparams, { state.skyboxcontext.rendercomplete });
  }

  state.resources.release(state.resourcetoken);
}
