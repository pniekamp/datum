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

  state.skybox = state.resources.create<Skybox>(state.assets.find(CoreAsset::default_skybox));

  state.testimage = state.resources.create<Sprite>(state.assets.find(CoreAsset::test_image));

  state.testplane = state.resources.create<Mesh>(state.assets.load(platform, "plane.pack"));
  state.testsphere = state.resources.create<Mesh>(state.assets.load(platform, "sphere.pack"));

  state.testenvmap = state.resources.create<EnvMap>(state.assets.find(CoreAsset::default_skybox));

  state.camera.set_position(Vec3(0.0f, 1.0f, 0.0f));

#if 1
  for(float smoothness = 0; smoothness < 1.0f + 1e-3f; smoothness += 0.1)
  {
    float x = smoothness * 12.0f;
    float y = 1.0f;
    float z = -5.0f;

    auto material = state.resources.create<Material>(Color3(1.0f, 0.0f, 0.0f), 0.0f, smoothness);

    auto entity = state.scene.create<Entity>();
    state.scene.add_component<TransformComponent>(entity, Transform::translation(Vec3(x, y, z)));
    state.scene.add_component<MeshComponent>(entity, state.testsphere, material, MeshComponent::Static | MeshComponent::Visible);
  }

  for(float smoothness = 0; smoothness < 1.0f + 1e-3f; smoothness += 0.1)
  {
    float x = smoothness * 12.0f;
    float y = 1.0f;
    float z = -3.0f;

    auto material = state.resources.create<Material>(Color3(1.000, 0.766, 0.336), 1.0f, smoothness);

    auto entity = state.scene.create<Entity>();
    state.scene.add_component<TransformComponent>(entity, Transform::translation(Vec3(x, y, z)));
    state.scene.add_component<MeshComponent>(entity, state.testsphere, material, MeshComponent::Static | MeshComponent::Visible);
  }
#endif
}


///////////////////////// game_update ///////////////////////////////////////
void datumtest_update(PlatformInterface &platform, GameInput const &input, float dt)
{
  BEGIN_TIMED_BLOCK(Update, Color3(1.0, 1.0, 0.4))

  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  state.time += dt;

  bool inputaccepted = false;

  update_debug_overlay(input, &inputaccepted);

  if (!inputaccepted)
  {
    if (input.mousebuttons[GameInput::Left].down())
    {
      state.camera.yaw(1.5f * (state.lastmousex - input.mousex), Vec3(0.0f, 1.0f, 0.0f));
      state.camera.pitch(1.5f * (state.lastmousey - input.mousey));
    }

    float speed = 0.02;

    if (input.modifiers & GameInput::Shift)
      speed *= 10;

    if (input.controllers[0].move_up.down() && !(input.modifiers & GameInput::Control))
      state.camera.offset(speed*Vec3(0.0f, 0.0f, -1.0f));

    if (input.controllers[0].move_down.down() && !(input.modifiers & GameInput::Control))
      state.camera.offset(speed*Vec3(0.0f, 0.0f, 1.0f));

    if (input.controllers[0].move_up.down() && (input.modifiers & GameInput::Control))
      state.camera.offset(speed*Vec3(0.0f, 1.0f, 0.0f));

    if (input.controllers[0].move_down.down() && (input.modifiers & GameInput::Control))
      state.camera.offset(speed*Vec3(0.0f, -1.0f, 0.0f));

    if (input.controllers[0].move_left.down())
      state.camera.offset(speed*Vec3(-1.0f, 0.0f, 0.0f));

    if (input.controllers[0].move_right.down())
      state.camera.offset(speed*Vec3(+1.0f, 0.0f, 0.0f));
  }

  state.lastmousex = input.mousex;
  state.lastmousey = input.mousey;
  state.lastmousez = input.mousez;

#ifdef DEBUG
  state.camera.set_exposure(debug_menu_value("Exposure", state.camera.exposure(), 0.0f, 8.0f));
#endif

  state.camera = normalise(state.camera);

  update_meshes(state.scene);

  state.writeframe->time = state.time;
  state.writeframe->camera = state.camera;

#if 1
  {
    CasterList::BuildState buildstate;

    if (state.writeframe->casters.begin(buildstate, platform, state.rendercontext, &state.resources))
    {
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

  DEBUG_MENU_ENTRY("Position", state.camera.position());
  DEBUG_MENU_ENTRY("Exposure", state.camera.exposure());

  state.writeframe->resourcetoken = state.resources.token();

  state.writeframe = state.readyframe.exchange(state.writeframe);

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

  if (!state.skybox->ready())
  {
    state.resources.request(platform, state.skybox);
  }

  if (!state.testenvmap->ready())
  {
    state.resources.request(platform, state.testenvmap);
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

//    overlay.push_sprite(buildstate, Vec2(400, 300), 300, state.testimage);

    overlay.finalise(buildstate);
  }

  renderlist.push_sprites(viewport, overlay);
#endif

//  renderlist.push_environment(Vec3(6, 12, 26), Transform::translation(0, 6, 0) * Transform::rotation(Vec3(0, 1, 0), -pi<float>()/2), state.testenvmap);

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
  renderparams.width = viewport.width;
  renderparams.height = viewport.height;
  renderparams.skybox = state.skybox;
  renderparams.sundirection = normalise(Vec3(0.4, -1, -0.1));
  renderparams.sunintensity = Color3(5, 5, 5);
  renderparams.skyboxorientation = Transform::rotation(Vec3(0.0f, 1.0f, 0.0f), -0.1*state.readframe->time);

  DEBUG_MENU_ENTRY("Sun Direction", renderparams.sundirection = normalise(debug_menu_value("Sun Direction", renderparams.sundirection, Vec3(-1), Vec3(1))))

  render_debug_overlay(platform, state.rendercontext, &state.resources, renderlist, viewport, state.debugfont);

  render(state.rendercontext, viewport, camera, renderlist, renderparams);

  state.resources.release(state.readframe->resourcetoken);

  END_TIMED_BLOCK(Render)
}
