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
    resources(&assets, allocator),
    scene(allocator)
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

  state.camera.set_projection(state.fov*pi<float>()/180.0f, state.aspect);

  state.scene.initialise_component_storage<NameComponent>();
  state.scene.initialise_component_storage<TransformComponent>();
  state.scene.initialise_component_storage<SpriteComponent>();
  state.scene.initialise_component_storage<MeshComponent>();
  state.scene.initialise_component_storage<LightComponent>();

  state.assets.load(platform, "core.pack");

  state.loader = state.resources.create<Sprite>(state.assets.find(CoreAsset::loader_image));

  state.debugfont = state.resources.create<Font>(state.assets.find(CoreAsset::debug_font));

  state.unitsphere = state.resources.create<Mesh>(state.assets.find(CoreAsset::unit_sphere));
  state.defaultmaterial = state.resources.create<Material>(state.assets.find(CoreAsset::default_material));

  state.testplane = state.resources.create<Mesh>(state.assets.load(platform, "plane.pack"));
  state.testsphere = state.resources.create<Mesh>(state.assets.load(platform, "sphere.pack"));

  state.camera.set_position(Vec3(0.0f, 1.0f, 0.0f));
}


///////////////////////// game_update ///////////////////////////////////////
void datumtest_update(PlatformInterface &platform, GameInput const &input, float dt)
{
  BEGIN_TIMED_BLOCK(Update, Color3(1.0, 1.0, 0.4))

  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  state.time += dt;

  if (input.mousebuttons[GameInput::Left].state == true)
  {
    state.camera.yaw(0.0025f * (state.lastmousex - input.mousex), Vec3(0.0f, 1.0f, 0.0f));
    state.camera.pitch(0.0025f * (state.lastmousey - input.mousey));
  }

  float speed = 0.02;

  if (input.modifiers & GameInput::Shift)
    speed *= 10;

  if (input.controllers[0].move_up.state == true && !(input.modifiers & GameInput::Control))
    state.camera.offset(speed*Vec3(0.0f, 0.0f, -1.0f));

  if (input.controllers[0].move_down.state == true && !(input.modifiers & GameInput::Control))
    state.camera.offset(speed*Vec3(0.0f, 0.0f, 1.0f));

  if (input.controllers[0].move_up.state == true && (input.modifiers & GameInput::Control))
    state.camera.offset(speed*Vec3(0.0f, 1.0f, 0.0f));

  if (input.controllers[0].move_down.state == true && (input.modifiers & GameInput::Control))
    state.camera.offset(speed*Vec3(0.0f, -1.0f, 0.0f));

  if (input.controllers[0].move_left.state == true)
    state.camera.offset(speed*Vec3(-1.0f, 0.0f, 0.0f));

  if (input.controllers[0].move_right.state == true)
    state.camera.offset(speed*Vec3(+1.0f, 0.0f, 0.0f));

  state.lastmousex = input.mousex;
  state.lastmousey = input.mousey;
  state.lastmousez = input.mousez;

  state.camera = normalise(state.camera);

  state.writeframe->time = state.time;
  state.writeframe->camera = state.camera;

#if 1
  {
    CasterList::BuildState buildstate;

    if (state.writeframe->casters.begin(buildstate, platform, state.rendercontext, &state.resources))
    {
      state.writeframe->casters.push_mesh(buildstate, Transform::translation(sin(state.time), 1, -5), state.testsphere, state.defaultmaterial);
      state.writeframe->casters.push_mesh(buildstate, Transform::translation(0, 1, -3 - sin(state.time)), state.testsphere, state.defaultmaterial);

      state.writeframe->casters.push_mesh(buildstate, Transform::identity(), state.testplane, state.defaultmaterial);

      for(auto &entity : state.scene.entities<MeshComponent>())
      {
        auto instance = state.scene.get_component<MeshComponent>(entity);
        auto transform = state.scene.get_component<TransformComponent>(entity);

        state.writeframe->casters.push_mesh(buildstate, transform.world(), instance.mesh(), instance.material());
      }

      state.writeframe->casters.finalise(buildstate);
    }
  }
#endif

#if 1
  {
    MeshList::BuildState buildstate;

    if (state.writeframe->meshes.begin(buildstate, platform, state.rendercontext, &state.resources))
    {
      state.writeframe->meshes.push_mesh(buildstate, Transform::translation(sin(state.time), 1, -5), state.testsphere, state.defaultmaterial);
      state.writeframe->meshes.push_mesh(buildstate, Transform::translation(0, 1, -3 - sin(state.time)), state.testsphere, state.defaultmaterial);

      state.writeframe->meshes.push_mesh(buildstate, Transform::identity(), state.testplane, state.defaultmaterial);

      for(auto &entity : state.scene.entities<MeshComponent>())
      {
        auto instance = state.scene.get_component<MeshComponent>(entity);
        auto transform = state.scene.get_component<TransformComponent>(entity);

        state.writeframe->meshes.push_mesh(buildstate, transform.world(), instance.mesh(), instance.material());
      }

      state.writeframe->meshes.finalise(buildstate);
    }
  }
#endif

#if 1
  {
    LightList::BuildState buildstate;

    if (state.writeframe->lights.begin(buildstate, platform, state.rendercontext, &state.resources))
    {
      state.writeframe->lights.push_pointlight(buildstate, Vec3(10*sin(0.4*state.time), 2, 2*sin(0.6*state.time)), 5, Color3(1, 0, 1), Attenuation(0, 0, 1));

      for(auto &entity : state.scene.entities<PointLightComponent>())
      {
        auto light = state.scene.get_component<PointLightComponent>(entity);
        auto transform = state.scene.get_component<TransformComponent>(entity);

        state.writeframe->lights.push_pointlight(buildstate, transform.world().translation(), light.range(), light.intensity(), light.attenuation());
      }

      state.writeframe->lights.finalise(buildstate);
    }
  }
#endif

#if 0
  {
    SpriteList::BuildState buildstate;

    if (state.writeframe->sprites.begin(buildstate, platform, state.rendercontext, &state.resources))
    {
      float count = 15.0f;
      float radius = 0.2f;

      for(float angle = 0.0f; angle < 2*pi<float>(); angle += pi<float>()/count)
      {
        Vec2 position = Vec2(0.5, 0.5) + radius * rotate(Vec2(1.0f, 0.0f), angle + state.time);

        state.writeframe->sprites.push_rect(buildstate, position, Rect2({0, -0.008}, {0.05, 0.008}), angle + state.time, Color4(1, 0, 0, 1));
      }

      state.writeframe->sprites.finalise(buildstate);
    }
  }
#endif

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
    render_fallback(state.rendercontext, viewport, embeded::logo.data, embeded::logo.width, embeded::logo.height);
    return;
  }

  while (state.readyframe.load()->time <= state.readframe->time)
    ;

  state.readframe = state.readyframe.exchange(state.readframe);

  BEGIN_TIMED_BLOCK(Render, Color3(0.0, 0.2, 1.0))

  auto &camera = state.readframe->camera;

  RenderList renderlist(platform.renderscratchmemory, 8*1024*1024);

  renderlist.push_meshes(state.readframe->meshes);
  renderlist.push_casters(state.readframe->casters);
  renderlist.push_lights(state.readframe->lights);
  renderlist.push_sprites(Rect2({ 0, 0.5f - 0.5f * viewport.height / viewport.width }, { 1, 0.5f + 0.5f * viewport.height / viewport.width }), state.readframe->sprites);

#if 1
  SpriteList overlay;
  SpriteList::BuildState buildstate;

  if (overlay.begin(buildstate, platform, state.rendercontext, &state.resources))
  {
    overlay.push_sprite(buildstate, Vec2(viewport.width - 30, 30), 40, state.loader, fmod(10*state.readframe->time, state.loader->layers));

    overlay.finalise(buildstate);
  }

  renderlist.push_sprites(viewport, overlay);
#endif

#ifdef DEBUG
  if (state.rendercontext.frame % 600 == 0)
  {
    cout << "Slots: " << g_debugstatistics.resourceslotsused << " / " << g_debugstatistics.resourceslotscapacity << "  ";
    cout << "Buffers: " << g_debugstatistics.resourcebufferused << " / " << g_debugstatistics.resourcebuffercapacity << "  ";
    cout << "Storage: " << g_debugstatistics.renderstorageused << " / " << g_debugstatistics.renderstoragecapacity << "  ";
    cout << "Lumps: " << g_debugstatistics.renderlumpsused << " / " << g_debugstatistics.renderlumpscapacity << "  ";
    cout << "Entities: " << g_debugstatistics.entityslotsused << " / " << g_debugstatistics.entityslotscapacity << "  ";
    cout << endl;
  }
#endif

  RenderParams renderparams;
  renderparams.skyboxblend = 0.9;//abs(sin(0.01*state.time));
  renderparams.sundirection = normalise(Vec3(renderparams.skyboxblend - 0.5, -1, -0.1));
  renderparams.sunintensity = Color3(renderparams.skyboxblend, renderparams.skyboxblend, renderparams.skyboxblend);
  renderparams.skyboxorientation = Quaternion3f(Vector3(0.0f, 1.0f, 0.0f), 0.1*state.time);

  render_debug_overlay(platform, state.rendercontext, &state.resources, renderlist, viewport, state.debugfont);

  render(state.rendercontext, viewport, camera, renderlist, renderparams);

  state.resources.release(state.readframe->resourcetoken);

  END_TIMED_BLOCK(Render)
}
