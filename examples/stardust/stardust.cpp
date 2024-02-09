//
// stardust.cpp
//

#include "stardust.h"
#include <leap/lml/matrixconstants.h>
#include "datum/debug.h"

using namespace std;
using namespace lml;
using namespace DatumPlatform;
using leap::extentof;

#define RND_GEN(x) (x = x * 196314165 + 907633515)

namespace
{
  void update_particle(uint32_t rnd, uint32_t rnd_mat, float tt, Vec3 *position, Color4 *color)
  {
    Vec4 p;
    float c = 0;

    rnd = rnd * 196314165u + 907633515u;
    p.x = float(rnd) * 2.3283064365387e-10f;
    rnd = rnd * 196314165u + 907633515u;
    p.y = float(rnd) * 2.3283064365387e-10f;
    rnd = rnd * 196314165u + 907633515u;
    p.z = float(rnd) * 2.3283064365387e-10f;
    p.w = 1.0;

    Vec3 t0, s0, r0, t1, s1, r1;

    // translation 0
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    t0.x = -1.3f + 2.6f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    t0.y = -1.3f + 2.6f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    t0.z = -1.3f + 2.6f * float(rnd_mat) * 2.3283064365387e-10f;

    // scaling 0
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    s0.x = 0.8f + 0.2f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    s0.y = 0.8f + 0.2f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    s0.z = 0.8f + 0.2f * float(rnd_mat) * 2.3283064365387e-10f;

    // rotation 0
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    r0.x = 1.57079632679f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    r0.y = 1.57079632679f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    r0.z = 1.57079632679f * float(rnd_mat) * 2.3283064365387e-10f;

    // translation 1
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    t1.x = -1.3f + 2.6f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    t1.y = -1.3f + 2.6f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    t1.z = -1.3f + 2.6f * float(rnd_mat) * 2.3283064365387e-10f;

    // scaling 1
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    s1.x = 0.8f + 0.2f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    s1.y = 0.8f + 0.2f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    s1.z = 0.8f + 0.2f * float(rnd_mat) * 2.3283064365387e-10f;

    // rotation 1
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    r1.x = 1.57079632679f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    r1.y = 1.57079632679f * float(rnd_mat) * 2.3283064365387e-10f;
    rnd_mat = rnd_mat * 196314165u + 907633515u;
    r1.z = 1.57079632679f * float(rnd_mat) * 2.3283064365387e-10f;

    float tmp0_ch = cos(r0.x);
    float tmp0_sh = sin(r0.x);
    float tmp0_cp = cos(r0.y);
    float tmp0_sp = sin(r0.y);
    float tmp0_cb = cos(r0.z);
    float tmp0_sb = sin(r0.z);

    float tmp1_ch = cos(r1.x);
    float tmp1_sh = sin(r1.x);
    float tmp1_cp = cos(r1.y);
    float tmp1_sp = sin(r1.y);
    float tmp1_cb = cos(r1.z);
    float tmp1_sb = sin(r1.z);

    float tt0 = 1.0f - tt;

    Matrix4f transform;
    transform(0, 0) = (tmp0_ch * tmp0_cb + tmp0_sh * tmp0_sp * tmp0_sb * s0.x) * tt0 + (tmp1_ch * tmp1_cb + tmp1_sh * tmp1_sp * tmp1_sb * s1.x) * tt;
    transform(1, 0) = (tmp0_sb * tmp0_cp) * tt0 + (tmp1_sb * tmp1_cp) * tt;
    transform(2, 0) = (-tmp0_sh * tmp0_cb + tmp0_ch * tmp0_sp * tmp0_sb) * tt0 + (-tmp1_sh * tmp1_cb + tmp1_ch * tmp1_sp * tmp1_sb) * tt;
    transform(3, 0) = 0.0;
    transform(0, 1) = (-tmp0_ch * tmp0_sb + tmp0_sh * tmp0_sp * tmp0_cb) * tt0 + (-tmp1_ch * tmp1_sb + tmp1_sh * tmp1_sp * tmp1_cb) * tt;
    transform(1, 1) = (tmp0_cb * tmp0_cp * s0.y) * tt0 + (tmp1_cb * tmp1_cp * s1.y) * tt;
    transform(2, 1) = (tmp0_sb * tmp0_sh + tmp0_ch * tmp0_sp * tmp0_cb) * tt0 + (tmp1_sb * tmp1_sh + tmp1_ch * tmp1_sp * tmp1_cb) * tt;
    transform(3, 1) = 0.0;
    transform(0, 2) = (tmp0_sh * tmp0_cp) * tt0 + (tmp1_sh * tmp1_cp) * tt;
    transform(1, 2) = (-tmp0_sp) * tt0 + (-tmp1_sp) * tt;
    transform(2, 2) = (tmp0_ch * tmp0_cp * s0.z) * tt0 + (tmp1_ch * tmp1_cp * s1.z) * tt;
    transform(3, 2) = 0.0;
    transform(0, 3) = t0.x * tt0 + t1.x * tt;
    transform(1, 3) = t0.y * tt0 + t1.y * tt;
    transform(2, 3) = t0.z * tt0 + t1.z * tt;
    transform(3, 3) = 1.0;

    for(int i = 0; i < 8; ++i)
    {
      p = transform * p;
      float radius = norm(p);
      float theta = p.y * (1.0f / p.x);
      p = Vec4(radius * cos(theta - radius), radius * sin(theta - radius), p.z, p.w);
      c += 0.1f * sin(theta);
    }

    *position = p.xyz;
    *color = clamp(Color4(1.4f - 5.0f*c*c, 0.2f - 2.0f*c, 1.0f + 4.0f*c, 1), 0.0f, 1.0f) * 0.03f;
  }

  struct WorkOrder
  {
    int i;
    int k0;
    int i0, i1;

    atomic<int> *latch;
  };

  void update_thread(DatumPlatform::PlatformInterface &platform, void *ldata, void *rdata)
  {
    auto &work = *static_cast<WorkOrder*>(rdata);
    auto &state = *static_cast<GameState*>(ldata);

    int k = work.k0;
    int i0 = work.i0;
    int i1 = work.i1;

    auto rnd = state.rnd;
    auto palettefactor = state.palettefactor * state.palettefactor  * (3.0f - 2.0f * state.palettefactor);

    for(int i = i0; i < i1; ++i)
    {
      auto &instance = state.particles[i];

      for(int j = 0; j < instance->count; ++j)
      {
        update_particle(state.seeds[++k], rnd, palettefactor, &instance->position[j], &instance->color[j]);
      }
    }

    auto &cmdlist = state.cmdlists[work.i];

    ForwardList::BuildState buildstate;

    if (cmdlist.begin(buildstate, state.rendercontext, state.resources))
    {
      for(int i = i0; i < i1; ++i)
      {
        cmdlist.push_particlesystem(buildstate, state.particlesystem, state.particles[i]);
      }

      cmdlist.finalise(buildstate);
    }

    *work.latch -= 1;
  }
}


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

  initialise_render_context(platform, state.rendercontext, 128*1024*1024, 0);

  state.camera.set_projection(state.fov*pi<float>()/180.0f, state.aspect);

  auto core = state.assets.load(platform, "core.pack");

  if (!core)
    throw runtime_error("Core Assets Load Failure");

  if (core->magic != CoreAsset::magic || core->version != CoreAsset::version)
    throw runtime_error("Core Assets Version Mismatch");

  state.debugfont = state.resources.create<Font>(state.assets.find(CoreAsset::debug_font));
  state.defaultmaterial = state.resources.create<Material>(state.assets.find(CoreAsset::default_material));

  auto pack = state.assets.load(platform, "stardust.pack");

  if (!pack)
    throw runtime_error("Data Assets Load Failure");

  state.skybox = state.resources.create<SkyBox>(state.assets.find(pack->id + 1));

  auto particle = state.resources.create<Texture>(state.assets.find(pack->id + 2), Texture::Format::SRGBA);

  state.particlesystem = state.resources.create<ParticleSystem>(1000, bound_limits<Bound3>::max(), particle);

  unsigned int seed = 23232323;

  for(auto &pseed : state.seeds)
  {
    RND_GEN(seed);
    pseed = seed;
  }

  for(auto &instance : state.particles)
  {
    instance = state.particlesystem->create(platform.gamememory);

    instance->count = 1000;

    for(int i = 0; i < instance->count; ++i)
    {
      instance->transform[i] = ScaleMatrix(0.1f, 0.1f);
      instance->emissive[i] = 1.0f;
    }
  }

  state.camera.lookat(Vec3(24.0f, 24.0f, 24.0f), Vec3(0, 0, 0), Vec3(0, 1, 0));

  state.mode = GameState::Startup;
}


///////////////////////// game_resize ///////////////////////////////////////
void example_resize(PlatformInterface &platform, Viewport const &viewport)
{
  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  if (state.rendercontext.ready)
  {
    RenderParams renderparams;
    renderparams.width = viewport.width;
    renderparams.height = viewport.height;
    renderparams.aspect = state.aspect;
    renderparams.ssaoscale = 0.0f;
    renderparams.fogdensity = 0.0f;

    prepare_render_pipeline(state.rendercontext, renderparams);
  }
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

    request(platform, state.resources, state.skybox, &ready, &total);
    request(platform, state.resources, state.particlesystem, &ready, &total);

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
      state.camera.yaw(-1.5f * input.deltamousex, Vec3(0, 1, 0));
      state.camera.pitch(-1.5f * input.deltamousey);
    }

    float speed = 0.05f;

    if (input.controllers[0].move_up.down())
      state.camera.offset(speed*Vec3(0, 0, -1));

    if (input.controllers[0].move_down.down())
      state.camera.offset(speed*Vec3(0, 0, 1));

    if (input.controllers[0].move_left.down())
      state.camera.offset(speed*Vec3(-1, 0, 0));

    if (input.controllers[0].move_right.down())
      state.camera.offset(speed*Vec3(1, 0, 0));

    state.camera = normalise(state.camera);

    state.palettefactor += dt * 0.25f;

    if (state.palettefactor > 1.0f)
    {
      for(int i = 0; i < 9; ++i)
      {
        RND_GEN(state.rnd);
      }

      state.palettefactor = 0.0f;
    }

    atomic<int> latch(ThreadCount);

    WorkOrder work[ThreadCount];
    for(int i = 0; i < ThreadCount; ++i)
    {
      work[i].i = i;
      work[i].k0 = i * 1000;
      work[i].i0 = i * extentof(state.particles) / ThreadCount;
      work[i].i1 = min(work[i].i0 + extentof(state.particles) / ThreadCount, extentof(state.particles));
      work[i].latch = &latch;
    }

    for(int i = 1; i < ThreadCount; ++i)
      platform.submit_work(update_thread, &state, &work[i]);

    update_thread(platform, &state, &work[0]);

    while (latch != 0)
      ;
  }
}


///////////////////////// game_render ///////////////////////////////////////
void example_render(DatumPlatform::PlatformInterface &platform, DatumPlatform::Viewport const &viewport)
{
  GameState &state = *static_cast<GameState*>(platform.gamememory.data);

  RenderParams renderparams;
  renderparams.width = viewport.width;
  renderparams.height = viewport.height;
  renderparams.aspect = state.aspect;
  renderparams.ssaoscale = 0.0f;
  renderparams.fogdensity = 0.0f;

  state.resourcetoken = state.resources.token();

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

    for(auto &cmdlist : state.cmdlists)
      renderlist.push_forward(cmdlist);

    renderparams.skybox = state.skybox;

    render(state.rendercontext, viewport, camera, renderlist, renderparams);
  }

  state.resources.release(state.resourcetoken);
}
