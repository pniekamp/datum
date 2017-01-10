//
// datumtest.cpp
//

#include "datumtest.h"
#include "fallback.h"
#include "datum/debug.h"

using namespace std;
using namespace lml;
using namespace DatumPlatform;

namespace
{

  ///////////////////////// random_lights ///////////////////////////////////
  void random_lights(Scene &scene, int count)
  {
    Bound3 bound = bound_limits<Bound3>::min();

    for(auto &entity : scene.entities<MeshComponent>())
    {
      bound = expand(bound, scene.get_component<MeshComponent>(entity).bound());
    }

    bound.min.y = 0.5;

    float maxradius = 3 * pow(volume(bound) / count, 1.0f/3.0f);
    float minradius = maxradius * 0.8;

    for(int i = 0; i < count; ++i)
    {
      float range = minradius + (maxradius - minradius) * rand() / float(RAND_MAX);

      Color3 intensity = hsv(360 * rand() / float(RAND_MAX), 1.0f, 0.4f + 0.3f * rand() / float(RAND_MAX));

      Attenuation attenuation = Attenuation(256*max_element(intensity)/(range*range), 0.0f, 1.0f);

      float rx = (bound.max.x - bound.min.x - range/4) * rand() / float(RAND_MAX);
      float ry = (bound.max.y - bound.min.y - range/4) * rand() / float(RAND_MAX);
      float rz = (bound.max.z - bound.min.z - range/4) * rand() / float(RAND_MAX);

      Vec3 position = bound.min + Vec3(rx, ry, rz);

      auto light = scene.create<Entity>();
      scene.add_component<TransformComponent>(light, Transform::translation(position));
      scene.add_component<PointLightComponent>(light, intensity, attenuation);
    }
  }

}


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

  initialise_asset_system(platform, state.assets, 64*1024, 256*1024*1024);

  initialise_resource_system(platform, state.resources, 2*1024*1024, 8*1024*1024, 64*1024*1024);

  initialise_resource_pool(platform, state.rendercontext.resourcepool, 16*1024*1024);

  state.camera.set_projection(state.fov*pi<float>()/180.0f, state.aspect);

  state.scene.initialise_component_storage<NameComponent>();
  state.scene.initialise_component_storage<TransformComponent>();
  state.scene.initialise_component_storage<SpriteComponent>();
  state.scene.initialise_component_storage<MeshComponent>();
  state.scene.initialise_component_storage<PointLightComponent>();
  state.scene.initialise_component_storage<ParticleSystemComponent>();

  auto core = state.assets.load(platform, "core.pack");

  if (core->magic != CoreAsset::magic || core->version != CoreAsset::version)
    throw runtime_error("Core Assets Version Mismatch");

  state.loader = state.resources.create<Sprite>(state.assets.find(CoreAsset::loader_image));

  state.debugfont = state.resources.create<Font>(state.assets.find(CoreAsset::debug_font));

  state.unitsphere = state.resources.create<Mesh>(state.assets.find(CoreAsset::unit_sphere));
  state.defaultmaterial = state.resources.create<Material>(state.assets.find(CoreAsset::default_material));

  state.skybox = state.resources.create<SkyBox>(state.assets.find(CoreAsset::default_skybox));

  state.testimage = state.resources.create<Sprite>(state.assets.find(CoreAsset::test_image), Vec2(0.0, 0.0));

  state.testplane = state.resources.create<Mesh>(state.assets.load(platform, "plane.pack"));
  state.testsphere = state.resources.create<Mesh>(state.assets.load(platform, "sphere.pack"));

  state.suzanne = unique_resource<Mesh>(&state.resources, state.resources.create<Mesh>(state.assets.load(platform, "suzanne.pack")));

  state.testparticlesystem = new(allocate<ParticleSystem>(platform.gamememory)) ParticleSystem({ platform.gamememory, state.particlefreelist });

  state.testparticlesystem->spritesheet = state.resources.create<Texture>(state.assets.find(CoreAsset::default_particle), Texture::Format::SRGBA);

  ParticleEmitter emitter;
  emitter.duration = 3.0f;
  emitter.rate = 0.0f;
  emitter.bursts = 1;
  emitter.bursttime[0] = 0.0f;
  emitter.burstcount[0] = 80;
  emitter.life = make_uniform_distribution(1.0f, 2.0f);
  emitter.looping = true;
  emitter.velocity = make_uniform_distribution(Vec3(5.0f, 0.0f, 0.0f), Vec3(20.0f, 0.0f, 0.0f));
  emitter.acceleration = Vec3(0.0f, -9.81f, 0.0f);
  emitter.size = Vec2(1.0f, 0.5f);
  emitter.scale = make_uniform_distribution(0.01f, 0.06f);
  emitter.rotation = 0;
  emitter.color = Color4(30, 10, 10, 1);
  emitter.modules |= ParticleEmitter::ShapeEmitter;
  emitter.shape = ParticleEmitter::Shape::Hemisphere;
  emitter.radius = 0.5f;
  emitter.angle = 15.0f / 180.0 * pi<float>();
  emitter.modules |= ParticleEmitter::StretchWithVelocity;
  emitter.modules |= ParticleEmitter::ColorOverLife;
  emitter.coloroverlife = make_colorfade_distribution(Color4(1.0f, 1.0f, 1.0f, 1.0f), 0.85f);
  state.testparticlesystem->emitters.push_back(emitter);

  state.testparticles = state.testparticlesystem->create();

//  while (!prepare_skybox_context(platform, state.skyboxcontext, &state.assets, 2))
//    ;

//  state.renderframes[0].skybox = state.resources.create<SkyBox>(512, 512, EnvMap::Format::FLOAT16);
//  state.renderframes[1].skybox = state.resources.create<SkyBox>(512, 512, EnvMap::Format::FLOAT16);
//  state.renderframes[2].skybox = state.resources.create<SkyBox>(512, 512, EnvMap::Format::FLOAT16);

  state.camera.set_position(Vec3(0.0f, 1.0f, 3.0f));

#if 0
  auto test = state.assets.load(platform, "test.pack");
  state.scene.load<Model>(platform, &state.resources, state.assets.find(test->id + 1));
#endif

#if 0
  state.scene.load<Model>(platform, &state.resources, state.assets.load(platform, "sponza.pack"));

  random_lights(state.scene, 128);

  auto light1 = state.scene.create<Entity>();
  state.scene.add_component<TransformComponent>(light1, Transform::translation(Vec3(4.85f, 1.45f, 1.45f)));
  state.scene.add_component<PointLightComponent>(light1, Color3(1.0f, 0.5f, 0.0f), Attenuation(0.4f, 0.0f, 1.0f));

  auto light2 = state.scene.create<Entity>();
  state.scene.add_component<TransformComponent>(light2, Transform::translation(Vec3(4.85f, 1.45f, -2.20f)));
  state.scene.add_component<PointLightComponent>(light2, Color3(1.0f, 0.3f, 0.0f), Attenuation(0.4f, 0.0f, 1.0f));

  auto light3 = state.scene.create<Entity>();
  state.scene.add_component<TransformComponent>(light3, Transform::translation(Vec3(-6.20f, 1.45f, -2.20f)));
  state.scene.add_component<PointLightComponent>(light3, Color3(1.0f, 0.5f, 0.0f), Attenuation(0.4f, 0.0f, 1.0f));

  auto light4 = state.scene.create<Entity>();
  state.scene.add_component<TransformComponent>(light4, Transform::translation(Vec3(-6.20f, 1.45f, 1.45f)));
  state.scene.add_component<PointLightComponent>(light4, Color3(1.0f, 0.4f, 0.0f), Attenuation(0.4f, 0.0f, 1.0f));

  state.camera.lookat(Vec3(0, 1, 0), Vec3(1, 1, 0), Vec3(0, 1, 0));
#endif

#if 1
  for(float roughness = 0; roughness < 1.0f + 1e-3f; roughness += 0.1f)
  {
    float x = (1 - roughness) * 12.0f;
    float y = 1.0f;
    float z = -5.0f;

    auto material = state.resources.create<Material>(Color3(1.0f, 0.0f, 0.0f), 0.0f, roughness);

    auto entity = state.scene.create<Entity>();
    state.scene.add_component<TransformComponent>(entity, Transform::translation(Vec3(x, y, z)));
    state.scene.add_component<MeshComponent>(entity, state.testsphere, material, MeshComponent::Static | MeshComponent::Visible);
  }

  for(float roughness = 0; roughness < 1.0f + 1e-3f; roughness += 0.1f)
  {
    float x = (1 - roughness) * 12.0f;
    float y = 1.0f;
    float z = -3.0f;

    auto material = state.resources.create<Material>(Color3(1.000f, 0.766f, 0.336f), 1.0f, roughness);

    auto entity = state.scene.create<Entity>();
    state.scene.add_component<TransformComponent>(entity, Transform::translation(Vec3(x, y, z)));
    state.scene.add_component<MeshComponent>(entity, state.testsphere, material, MeshComponent::Static | MeshComponent::Visible);
  }
#endif
}


///////////////////////// game_update ///////////////////////////////////////
void datumtest_update(PlatformInterface &platform, GameInput const &input, float dt)
{
  BEGIN_TIMED_BLOCK(Update, Color3(1.0f, 1.0f, 0.4f))

  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  state.time += dt;

  bool inputaccepted = false;

  update_debug_overlay(input, &inputaccepted);

  if (!inputaccepted)
  {
    if (input.mousebuttons[GameInput::Left].down())
    {
      state.camera.yaw(1.5f * (state.lastmousex - input.mousex), Vec3(0, 1, 0));
      state.camera.pitch(1.5f * (state.lastmousey - input.mousey));
    }

    float speed = 0.02f;

    if (input.modifiers & GameInput::Shift)
      speed *= 10;

    if (input.controllers[0].move_up.down() && !(input.modifiers & GameInput::Control))
      state.camera.offset(speed*Vec3(0, 0, -1));

    if (input.controllers[0].move_down.down() && !(input.modifiers & GameInput::Control))
      state.camera.offset(speed*Vec3(0, 0, 1));

    if (input.controllers[0].move_up.down() && (input.modifiers & GameInput::Control))
      state.camera.offset(speed*Vec3(0, 1, 0));

    if (input.controllers[0].move_down.down() && (input.modifiers & GameInput::Control))
      state.camera.offset(speed*Vec3(0, -1, 0));

    if (input.controllers[0].move_left.down())
      state.camera.offset(speed*Vec3(-1, 0, 0));

    if (input.controllers[0].move_right.down())
      state.camera.offset(speed*Vec3(1, 0, 0));
  }

  state.lastmousex = input.mousex;
  state.lastmousey = input.mousey;
  state.lastmousez = input.mousez;

#ifdef DEBUG
  state.luminancetarget = debug_menu_value("Camera/LumaTarget", state.luminancetarget, 0.0f, 8.0f);
#endif

  state.camera = adapt(state.camera, state.rendercontext.luminance, state.luminancetarget, 0.5f*dt);

  state.camera = normalise(state.camera);

  update_mesh_bounds(state.scene);
  update_particlesystem_bounds(state.scene);

  asset_guard lock(&state.assets);

  state.writeframe->time = state.time;
  state.writeframe->camera = state.camera;

  float suzannemetalness = 0.0f;
  DEBUG_MENU_VALUE("Suzanne/Metalness", &suzannemetalness, 0.0f, 1.0f)

  float suzanneroughness = 1.0f;
  DEBUG_MENU_VALUE("Suzanne/Roughness", &suzanneroughness, 0.0f, 1.0f)

  float suzannereflectivity = 0.5f;
  DEBUG_MENU_VALUE("Suzanne/Reflectivity", &suzannereflectivity, 0.0f, 8.0f)

  float suzanneemissive = 0.0f;
  DEBUG_MENU_VALUE("Suzanne/Emissive", &suzanneemissive, 0.0f, 128.0f)

  state.suzannematerial = unique_resource<Material>(&state.resources, state.resources.create<Material>(Color3(1, 0, 0), suzannemetalness, suzanneroughness, suzannereflectivity, cbrt(suzanneemissive/128)));

  float floormetalness = 0.0f;
  DEBUG_MENU_VALUE("Floor/Metalness", &floormetalness, 0.0f, 1.0f)

  float floorroughness = 1.0f;
  DEBUG_MENU_VALUE("Floor/Roughness", &floorroughness, 0.0f, 1.0f)

  state.floormaterial = unique_resource<Material>(&state.resources, state.resources.create<Material>(Color3(0.64f, 0.64f, 0.64f), floormetalness, floorroughness));

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
      state.writeframe->meshes.push_mesh(buildstate, Transform::translation(-3, 1, -3)*Transform::rotation(Vec3(0, 1, 0), state.time), state.suzanne, state.suzannematerial);

      state.writeframe->meshes.push_mesh(buildstate, Transform::identity(), state.testplane, state.floormaterial);

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

  SkyboxParams skyboxparams;
  skyboxparams.sunintensity = state.sunintensity;
  skyboxparams.sundirection = state.sundirection;

  DEBUG_MENU_VALUE("Lighting/Sky", &skyboxparams.skycolor, Color3(0, 0, 0), Color3(10, 10, 10));
  DEBUG_MENU_VALUE("Lighting/Ground", &skyboxparams.groundcolor, Color3(0, 0, 0), Color3(10, 10, 10));
  DEBUG_MENU_VALUE("Lighting/Sun Intensity", &state.sunintensity, Color3(0, 0, 0), Color3(10, 10, 10));
  DEBUG_MENU_ENTRY("Lighting/Sun Direction", state.sundirection = normalise(debug_menu_value("Lighting/Sun Direction", state.sundirection, Vec3(-1), Vec3(1))));

//  render_skybox(state.skyboxcontext, state.writeframe->skybox, skyboxparams);

  DEBUG_MENU_ENTRY("Camera/Position", state.camera.position());
  DEBUG_MENU_ENTRY("Camera/Exposure", state.camera.exposure());
  DEBUG_MENU_ENTRY("Camera/LumaTarget", state.luminancetarget);
  DEBUG_MENU_ENTRY("Camera/Luminance", state.rendercontext.luminance);

  state.writeframe->resourcetoken = state.resources.token();

  state.writeframe = state.readyframe.exchange(state.writeframe);

  END_TIMED_BLOCK(Update)

  stream_debuglog("debuglog.dump");
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
    asset_guard lock(&state.assets);

    state.resources.request(platform, state.skybox);
  }

  while (state.readyframe.load()->time <= state.readframe->time)
    ;

  state.readframe = state.readyframe.exchange(state.readframe);

  BEGIN_TIMED_BLOCK(Render, Color3(0.0f, 0.2f, 1.0f))

  asset_guard lock(&state.assets);

  auto &camera = state.readframe->camera;

  RenderList renderlist(platform.renderscratchmemory, 8*1024*1024);

  renderlist.push_meshes(state.readframe->meshes);
  renderlist.push_casters(state.readframe->casters);
  renderlist.push_lights(state.readframe->lights);
  renderlist.push_sprites(Rect2({ 0, 0.5f - 0.5f * viewport.height / viewport.width }, { 1, 0.5f + 0.5f * viewport.height / viewport.width }), state.readframe->sprites);

#if 1

  {
    Vec3 location(16.0f, 1.0f, -4.0f);
    DEBUG_MENU_VALUE("Particles/location", &location, Vec3(-15.0f), Vec3(15.0f));

    Vec3 rotation(0.0f, 0.0f, pi<float>()/2);
    DEBUG_MENU_VALUE("Particles/rotation", &rotation, Vec3(0.0f), Vec3(6.28f));

    auto transform = Transform::translation(location) * Transform::rotation(Vec3(1, 0, 0), rotation.x) * Transform::rotation(Vec3(0, 1, 0), rotation.y) * Transform::rotation(Vec3(0, 0, 1), rotation.z);

    state.testparticlesystem->update(state.testparticles, camera, transform, 1.0f/60.0f);
  }

  {
    ForwardList objects;
    ForwardList::BuildState buildstate;

    if (objects.begin(buildstate, platform, state.rendercontext, &state.resources))
    {
      objects.push_translucent(buildstate, Transform::translation(-8, 1, -1.5f)*Transform::rotation(Vec3(0, 1, 0), 5.0f), state.testsphere, state.suzannematerial, 0.5f+0.5f*sin(0.1f*state.time));

      objects.push_particlesystem(buildstate, state.testparticles);

      objects.finalise(buildstate);
    }

    renderlist.push_objects(objects);
  }
#endif

#if 0
  {
    OverlayList overlays;
    OverlayList::BuildState buildstate;

    if (overlays.begin(buildstate, platform, state.rendercontext, &state.resources))
    {
      overlays.push_wireframe(buildstate, Transform::translation(-2, 1, 0)*Transform::rotation(Vec3(0, 1, 0), 5.0f), state.suzanne, Color4(0.1f, 0.1f, 0.1f, 1.0f));

      overlays.push_stencil(buildstate, Transform::translation(-2, 1, 0)*Transform::rotation(Vec3(0, 1, 0), 5.0f), state.suzanne, state.suzannematerial);
      overlays.push_outline(buildstate, Transform::translation(-2, 1, 0)*Transform::rotation(Vec3(0, 1, 0), 5.0f), state.suzanne, state.suzannematerial, Color4(1.0f, 0.5f, 0.15f, 1.0f));

      overlays.push_gizmo(buildstate, camera.transform() * Transform::translation(10, 6, -12)*Transform::rotation(Vec3(1, 0, 0), 0.4f)*Transform::rotation(Vec3(0, 1, 0), state.readframe->time), Vec3(0.5f), state.suzanne, state.suzannematerial);

      overlays.finalise(buildstate);
    }

    renderlist.push_overlays(overlays);
  }
#endif

#if 1
  {
    SpriteList sprites;
    SpriteList::BuildState buildstate;

    if (sprites.begin(buildstate, platform, state.rendercontext, &state.resources))
    {
      sprites.push_sprite(buildstate, Vec2(viewport.width - 30, 50), 40, state.loader, fmod(10*state.readframe->time, state.loader->layers));

  //    overlay.push_sprite(buildstate, Vec2(400, 300), 300, state.testimage);

      sprites.finalise(buildstate);
    }

    renderlist.push_sprites(viewport, sprites);
  }
#endif

//  renderlist.push_environment(Vec3(6, 12, 26), Transform::translation(0, 6, 0) * Transform::rotation(Vec3(0, 1, 0), -pi<float>()/2), state.testenvmap);

  RenderParams renderparams;
  renderparams.width = viewport.width;
  renderparams.height = viewport.height;
//  renderparams.scale = 0.5f;
  renderparams.aspect = state.aspect;
  renderparams.skybox = state.skybox;
//  renderparams.skybox = state.readframe->skybox;
  renderparams.sundirection = state.sundirection;
  renderparams.sunintensity = state.sunintensity;
  //renderparams.skyboxorientation = Transform::rotation(Vec3(0, 1, 0), -0.1*state.readframe->time);
  renderparams.ssaoscale = 0.5f;

  DEBUG_MENU_VALUE("Lighting/SSR Strength", &renderparams.ssrstrength, 0.0f, 8.0f);
  DEBUG_MENU_VALUE("Lighting/Bloom Strength", &renderparams.bloomstrength, 0.0f, 18.0f);

  render_debug_overlay(platform, state.rendercontext, &state.resources, renderlist, viewport, state.debugfont);

  render(state.rendercontext, viewport, camera, renderlist, renderparams);

  state.resources.release(state.readframe->resourcetoken);

  END_TIMED_BLOCK(Render)
}
