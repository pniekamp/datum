//
// datumtest.cpp
//

#include "datumtest.h"
#include "datum/debug.h"

using namespace std;
using namespace lml;
using namespace DatumPlatform;


///////////////////////// GameState::Constructor ////////////////////////////
GameState::GameState(StackAllocator<> const &allocator)
  : assets(allocator)
{
}


///////////////////////// game_init /////////////////////////////////////////
void datumtest_init(PlatformInterface &platform)
{
  cout << "Init" << endl;

  GameState &state = *new(allocate<GameState>(platform.gamememory)) GameState(platform.gamememory);

  assert(&state == platform.gamememory.data);

  initialise_asset_system(platform, state.assets);

  state.assets.load(platform, "core.pack");
}


///////////////////////// game_update ///////////////////////////////////////
void datumtest_update(PlatformInterface &platform, GameInput const &input, float dt)
{
  BEGIN_TIMED_BLOCK(Update, Color3(1.0, 1.0, 0.4))

  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  state.time += dt;

  update_debug_overlay(input);

  END_TIMED_BLOCK(Update)
}


///////////////////////// game_render ///////////////////////////////////////
void datumtest_render(PlatformInterface &platform, Viewport const &viewport)
{
  BEGIN_FRAME()

//  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

//  if (!prepare_render_context(platform, state.renderercontext, &state.assets))
//    return;

  BEGIN_TIMED_BLOCK(Render, Color3(0.0, 0.2, 1.0))


  END_TIMED_BLOCK(Render)
}
