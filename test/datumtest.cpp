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

  state.scene.initialise_component_storage<NameComponent>();
  state.scene.initialise_component_storage<TransformComponent>();
  state.scene.initialise_component_storage<SpriteComponent>();
  state.scene.initialise_component_storage<MeshComponent>();

  state.assets.load(platform, "core.pack");

  state.loader = state.resources.create<Sprite>(state.assets.find(CoreAsset::loader_image));

  state.debugfont = state.resources.create<Font>(state.assets.find(CoreAsset::debug_font));

  state.unitsphere = state.resources.create<Mesh>(state.assets.find(CoreAsset::unit_sphere));
  state.defaultmaterial = state.resources.create<Material>(state.assets.find(CoreAsset::default_material));

  state.testsprite = state.resources.create<Sprite>(state.assets.find(CoreAsset::test_image));

  state.testmaterial = state.resources.create<Material>(state.assets.find(CoreAsset::default_material));

  state.testmesh = state.resources.create<Mesh>(state.assets.find(CoreAsset::unit_quad));

#if 0
  state.scene.load<Model>(platform, &state.resources, state.assets.load(platform, "sponza.pack"));
#endif

#if 0
  auto id = state.assets.load(platform, "test.pack")->id;

  auto model = state.scene.get<Model>(state.scene.create<Model>(&state.resources));

  model->add_mesh(state.resources.create<Mesh>(state.assets.find(id + 1))); // floor
  model->add_mesh(state.resources.create<Mesh>(state.assets.find(id + 2))); // teapot
  model->add_material(state.resources.create<Material>(state.assets.find(id + 3))); // wood
  model->add_material(state.resources.create<Material>(state.assets.find(id + 7))); // plastic
  model->add_texture(state.resources.create<Texture>(state.assets.find(id + 9), Texture::Format::sRGB8_A8)); // plastic specular
  model->add_texture(state.resources.create<Texture>(state.assets.find(id + 10), Texture::Format::RGBA8)); // plastic normal

  model->add_instance(Transform::translation(0, 0, 0), 0, 0, MeshComponent::Visible | MeshComponent::Static);

  const int NTEAPOTS = 7;
  const int HALF_NTEAPOTS = NTEAPOTS / 2;
  for(int x = 0; x < NTEAPOTS; ++x)
  {
    for(int z = 0; z < NTEAPOTS; ++z)
    {
      Vec3 position = Vec3(2 * (x - HALF_NTEAPOTS), 0.0f, 2 * (z - HALF_NTEAPOTS));

      float rotation = -38.0f * pi<float>()/180;

      Color3 color = Color3(x / (float)NTEAPOTS, 0.3f, z / (float)NTEAPOTS);

      auto material = state.resources.create<Material>(color, (Texture const *)nullptr, Color3(1.0, 1.0, 1.0), 90.0f, model->textures[0], model->textures[1]);

      auto mat = model->add_material(material);

      model->add_instance(Transform::translation(position) * Transform::rotation(Vec3(0, 1, 0), rotation), 1, mat, MeshComponent::Visible);
    }
  }
#endif

  state.camera.set_projection(state.fov*pi<float>()/180.0f, state.aspect);
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

  renderlist.push_sprites(Rect2({ 0, 0.5f - 0.5f * viewport.height / viewport.width }, { 1, 0.5f + 0.5f * viewport.height / viewport.width }), state.readframe->sprites);

#if 1
  MeshList meshes;
  MeshList::BuildState meshstate;

  if (meshes.begin(meshstate, platform, state.rendercontext, &state.resources))
  {
//    meshes.push_mesh(meshstate, Transform::translation(0, 0, -3), state.testmesh, state.testmaterial);
//    meshes.push_mesh(meshstate, Transform::translation(0, 0, -3 - sin(state.time)), state.unitsphere, state.defaultmaterial);

    for(auto &entity : state.scene.entities<MeshComponent>())
    {
      auto instance = state.scene.get_component<MeshComponent>(entity);
      auto transform = state.scene.get_component<TransformComponent>(entity);

      meshes.push_mesh(meshstate, transform.world(), instance.mesh(), instance.material());
    }

    meshes.finalise(meshstate);
  }

  renderlist.push_meshes(meshes);
#endif

#if 1
  SpriteList overlay;
  SpriteList::BuildState buildstate;

  if (overlay.begin(buildstate, platform, state.rendercontext, &state.resources))
  {
    overlay.push_sprite(buildstate, Vec2(viewport.width - 30, 30), 40, state.loader, fmod(10*state.readframe->time, state.loader->layers));

//    for(int i = 0; i < 50000; ++i)
      overlay.push_rect(buildstate, Vec2(10, 200), Rect2({0,0}, {50,50}), Color4(1, 1, 1, 1));

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
