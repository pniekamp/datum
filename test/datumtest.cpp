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

  initialise_asset_system(platform, state.assets, 64*1024, 128*1024*1024);

  initialise_resource_system(platform, state.resources, 2*1024*1024, 8*1024*1024, 64*1024*1024);

  initialise_resource_pool(platform, state.rendercontext.resourcepool, 16*1024*1024);

  state.assets.load(platform, "core.pack");

//  while (!prepare_render_context(platform, state.rendercontext, &state.assets))
//    ;

  state.testsprite = state.resources.create<Sprite>(state.assets.find(CoreAsset::test_image));

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

///
  SpriteList::BuildState buildstate;

  if (state.writeframe->sprites.begin(buildstate, platform, state.rendercontext, &state.resources))
  {
    state.writeframe->sprites.push_rect(buildstate, Vec2(10, 10), Rect2({0,0}, {100, 100}), Color4(1, 0, 0, 1));
    state.writeframe->sprites.push_rect(buildstate, Vec2(120, 10), Rect2({0,0}, {33, 33}), Color4(1, 0, 1, 1));
    state.writeframe->sprites.push_rect(buildstate, Vec2(160, 10), Rect2({0,0}, {33, 33}), Color4(1, 1, 0, 1));
    state.writeframe->sprites.push_rect(buildstate, Vec2(5, 15), Rect2({0,0}, {200, 40}), Color4(0, 1, 1, 0.4));

    state.writeframe->sprites.push_line(buildstate, Vec2(5, 5), Vec2(200, 150), Color4(1, 1, 1, 1));
    state.writeframe->sprites.push_line(buildstate, Vec2(5, 5), Vec2(200, 200), Color4(1, 1, 1, 1));
    state.writeframe->sprites.push_line(buildstate, Vec2(5, 200), Vec2(200, 5), Color4(1, 1, 1, 0.5), 20);

    state.writeframe->sprites.push_rect_outline(buildstate, Vec2(300, 300), Rect2({0,0}, {300, 200}), Color4(1, 1, 1, 0.5), 20);
    state.writeframe->sprites.push_rect_outline(buildstate, Vec2(300, 300), Rect2({0,0}, {300, 200}), -1.2f, Color4(1, 1, 1, 0.5), 20);

    state.writeframe->sprites.push_sprite(buildstate, Vec2(600, 200), 281.0f, state.time, state.testsprite);

    state.writeframe->sprites.push_rect(buildstate, Vec2(600, 200), Rect2({ -5, -5 }, { 5, 5 }), Color4(1, 0, 1, 1));

    state.writeframe->sprites.finalise(buildstate);
  }
///

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

  while (state.readyframe.load()->time <= state.readframe->time)
    ;

  state.readframe = state.readyframe.exchange(state.readframe);

  BEGIN_TIMED_BLOCK(Render, Color3(0.0, 0.2, 1.0))

  auto &camera = state.readframe->camera;

  PushBuffer pushbuffer(platform.renderscratchmemory, 8*1024*1024);

  if (state.readframe->sprites)
  {
    auto entry = pushbuffer.push<Renderable::Sprites>();

    if (entry)
    {
      entry->spritelist = state.readframe->sprites;
    }
  }

#ifdef DEBUG
  if (state.rendercontext.frame % 600 == 0)
  {
    cout << "Slots: " << g_debugstatistics.resourceslotsused << " / " << g_debugstatistics.resourceslotscapacity << "  ";
    cout << "Buffers: " << g_debugstatistics.resourcebufferused << " / " << g_debugstatistics.resourcebuffercapacity << "  ";
    cout << "Storage: " << g_debugstatistics.renderstorageused << " / " << g_debugstatistics.renderstoragecapacity << "  ";
    cout << "Lumps: " << g_debugstatistics.renderlumpsused << " / " << g_debugstatistics.renderlumpscapacity << "  ";
    cout << endl;
  }
#endif

  RenderParams renderparams;
  renderparams.skyboxblend = 0.9;//abs(sin(0.01*state.time));
  renderparams.sundirection = normalise(Vec3(renderparams.skyboxblend - 0.5, -1, -0.1));
  renderparams.sunintensity = Color3(renderparams.skyboxblend, renderparams.skyboxblend, renderparams.skyboxblend);
  renderparams.skyboxorientation = Quaternion3f(Vector3(0.0f, 1.0f, 0.0f), 0.1*state.time);

  render(state.rendercontext, viewport, camera, pushbuffer, renderparams);

  state.resources.release(state.readframe->resourcetoken);

  END_TIMED_BLOCK(Render)
}
