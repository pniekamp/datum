//
// datumtest.cpp
//

#include "datumtest.h"
#include "fallback.h"
#include "datum/debug.h"

using namespace std;
using namespace lml;
using namespace DatumPlatform;


///////////////////////// GameState::Constructor ////////////////////////////
GameState::GameState(StackAllocator<> const &allocator)
  : assets(allocator),
    resources(&assets, allocator)
{
  readframe = &renderframes[0];
  writeframe = &renderframes[1];
  readyframe = &renderframes[2];
}


///////////////////////// game_init /////////////////////////////////////////
void datumtest_init(PlatformInterface &platform)
{
  cout << "Init" << endl;

  GameState &state = *new(allocate<GameState>(platform.gamememory)) GameState(platform.gamememory);

  assert(&state == platform.gamememory.data);

  initialise_asset_system(platform, state.assets);

  initialise_resource_system(platform, state.resources);

  state.assets.load(platform, "core.pack");

//  while (!prepare_render_context(platform, state.rendercontext, &state.assets))
//    ;

  state.camera.set_projection(state.fov*pi<float>()/180.0f, state.aspect);
}


///////////////////////// game_update ///////////////////////////////////////
void datumtest_update(PlatformInterface &platform, GameInput const &input, float dt)
{
  BEGIN_TIMED_BLOCK(Update, Color3(1.0, 1.0, 0.4))

  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  state.time += dt;

  if (input.mousebuttons[GameInput::MouseLeft].state == true)
  {
    state.camera.yaw(0.0025f * (state.lastmousex - input.mousex), Vec3(0.0f, 1.0f, 0.0f));
    state.camera.pitch(0.0025f * (input.mousey - state.lastmousey));
  }

  state.lastmousex = input.mousex;
  state.lastmousey = input.mousey;
  state.lastmousez = input.mousez;

  state.camera = normalise(state.camera);

  state.writeframe->time = state.time;
  state.writeframe->camera = state.camera;

  BEGIN_STAT_BLOCK(build)

  SpriteList::BuildState buildstate;

  if (state.writeframe->sprites.begin(buildstate, platform, state.rendercontext, &state.resources))
  {
    float count = 15.0f;
    float radius = 150.0f;

    for(float angle = 0.0f; angle < 2*pi<float>(); angle += pi<float>()/count)
    {
      Vec2 position = Vec2(960/2, 540/2) + radius * rotate(Vec2(1.0f, 0.0f), angle + state.time);

      state.writeframe->sprites.push_sprite(buildstate, position, Rect2({0, -5}, {25, 5}), angle + state.time);
    }

    state.writeframe->sprites.finalise(buildstate);
  }

  END_STAT_BLOCK(build)

  state.writeframe->resourcetoken = state.resources.token();

  state.writeframe = state.readyframe.exchange(state.writeframe);

  update_debug_overlay(input);

  END_TIMED_BLOCK(Update)
}


///////////////////////// game_render ///////////////////////////////////////
void datumtest_render(PlatformInterface &platform, Viewport const &viewport)
{
  BEGIN_FRAME()

  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  if (!prepare_render_context(platform, state.rendercontext, &state.assets))
  {
    render_fallback(state.rendercontext, viewport, embeded::loading.data, embeded::loading.width, embeded::loading.height);
    return;
  }

  if (state.readframe->time < state.readyframe.load()->time)
  {
    state.readframe = state.readyframe.exchange(state.readframe);
  }

  BEGIN_TIMED_BLOCK(Render, Color3(0.0, 0.2, 1.0))

  auto &camera = state.readframe->camera;

  PushBuffer pushbuffer(platform.renderscratchmemory, 8*1024*1024);

  if (state.readframe->sprites)
  {
    auto entry = pushbuffer.push<Renderable::Rects>();

    if (entry)
    {
      entry->commandlist = state.readframe->sprites;
    }
  }

///
  if (state.rendercontext.frame % 600 == 0)
  {
    cout << g_debugstatistics.resourceslotsused << " / " << g_debugstatistics.resourceslotscapacity << " , " << g_debugstatistics.resourceblockssused << " / " << g_debugstatistics.resourceblockscapacity << "  ";
    cout << g_debugstatistics.storageused << " / " << g_debugstatistics.storagecapacity << " , "  << g_debugstatistics.lumpsused << " / " << g_debugstatistics.lumpscapacity << "  ";
    cout << endl;
  }
///

  RenderParams renderparams;
  renderparams.skyboxblend = 0.9;//abs(sin(0.01*state.time));
  renderparams.sundirection = normalise(Vec3(renderparams.skyboxblend - 0.5, -1, -0.1));
  renderparams.sunintensity = Color3(renderparams.skyboxblend, renderparams.skyboxblend, renderparams.skyboxblend);
  renderparams.skyboxorientation = Quaternion3f(Vector3(0.0f, 1.0f, 0.0f), 0.1*state.time);

  render(state.rendercontext, viewport, camera, pushbuffer, renderparams);

  state.resources.release(state.readframe->resourcetoken);

  END_TIMED_BLOCK(Render)
}
