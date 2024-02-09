//
// asteroids.cpp
//

#include "asteroids.h"
#include "datum/debug.h"

using namespace std;
using namespace lml;
using namespace DatumPlatform;
using leap::extentof;

namespace
{
  struct Icosahedron
  {
    size_t subdivs = 0;
    vector<Vec3> vertices[8];
    vector<uint32_t> indices[8];
  };

  void subdivide(vector<Vec3> &vertices, vector<uint32_t> &indices)
  {
    auto indexcount = indices.size();

    vector<uint32_t> newindices;

    vertices.reserve(vertices.size() * 2);
    newindices.reserve(indices.size() * 4);

    for(size_t i = 0; i != indexcount; i += 3)
    {
      auto t0 = indices[i];
      auto t1 = indices[i+1];
      auto t2 = indices[i+2];

      auto v0 = normalise(vertices[t0] + vertices[t1]);
      auto m0 = find(vertices.begin(), vertices.end(), v0);
      if (m0 == vertices.end())
        m0 = vertices.insert(vertices.end(), v0);
      auto i0 = m0 - vertices.begin();

      auto v1 = normalise(vertices[t1] + vertices[t2]);
      auto m1 = find(vertices.begin(), vertices.end(), v1);
      if (m1 == vertices.end())
        m1 = vertices.insert(vertices.end(), v1);
      auto i1 = m1 - vertices.begin();

      auto v2 = normalise(vertices[t2] + vertices[t0]);
      auto m2 = find(vertices.begin(), vertices.end(), v2);
      if (m2 == vertices.end())
        m2 = vertices.insert(vertices.end(), v2);
      auto i2 = m2 - vertices.begin();

      newindices.push_back(t0);
      newindices.push_back(i0);
      newindices.push_back(i2);
      newindices.push_back(i0);
      newindices.push_back(t1);
      newindices.push_back(i1);
      newindices.push_back(i0);
      newindices.push_back(i1);
      newindices.push_back(i2);
      newindices.push_back(i2);
      newindices.push_back(i1);
      newindices.push_back(t2);
    }

    swap(indices, newindices);
  }

  void calculate_basis(Mesh::Vertex *vertices, size_t vertexcout, uint32_t const *indices, size_t indexcount)
  {
    vector<Vec3> tan1(vertexcout, Vec3(0));
    vector<Vec3> tan2(vertexcout, Vec3(0));
    vector<Vec3> norm(vertexcout, Vec3(0));

    for(size_t i = 0; i < indexcount; i += 3)
    {
      auto &v1 = vertices[indices[i+0]];
      auto &v2 = vertices[indices[i+1]];
      auto &v3 = vertices[indices[i+2]];

      auto x1 = v2.position[0] - v1.position[0];
      auto x2 = v3.position[0] - v1.position[0];
      auto y1 = v2.position[1] - v1.position[1];
      auto y2 = v3.position[1] - v1.position[1];
      auto z1 = v2.position[2] - v1.position[2];
      auto z2 = v3.position[2] - v1.position[2];

      auto s1 = v2.texcoord[0] - v1.texcoord[0];
      auto s2 = v3.texcoord[0] - v1.texcoord[0];
      auto t1 = v1.texcoord[1] - v2.texcoord[1];
      auto t2 = v1.texcoord[1] - v3.texcoord[1];

      auto r = s1 * t2 - s2 * t1;

      if (r != 0)
      {
        auto sdir = Vec3(t2 * x1 - t1 * x2, t2 * y1 - t1 * y2, t2 * z1 - t1 * z2) / r;
        auto tdir = Vec3(s1 * x2 - s2 * x1, s1 * y2 - s2 * y1, s1 * z2 - s2 * z1) / r;

        auto uvarea = area(Vec2(v1.texcoord[0], v1.texcoord[1]), Vec2(v2.texcoord[0], v2.texcoord[1]), Vec2(v3.texcoord[0], v3.texcoord[1]));

        tan1[indices[i+0]] += sdir * uvarea;
        tan1[indices[i+1]] += sdir * uvarea;
        tan1[indices[i+2]] += sdir * uvarea;

        tan2[indices[i+0]] += tdir * uvarea;
        tan2[indices[i+1]] += tdir * uvarea;
        tan2[indices[i+2]] += tdir * uvarea;

        auto n = cross(Vec3(x1, y1, z1), Vec3(x2, y2, z2));

        norm[indices[i+0]] += n * uvarea;
        norm[indices[i+1]] += n * uvarea;
        norm[indices[i+2]] += n * uvarea;
      }
    }

    for(size_t i = 0; i < vertexcout; ++i)
    {
      auto normal = norm[i];
      auto tangent = tan1[i];
      auto bitangent = tan2[i];

      orthonormalise(normal, tangent, bitangent);

      vertices[i].normal = normal;
      vertices[i].tangent = Vec4(tangent, (dot(bitangent, tan2[i]) < 0.0f) ? -1.0f : 1.0f);
    }
  }

  Icosahedron make_icosahedron(int subdivs)
  {
    float a = sqrt(2.0f / (5.0f - sqrt(5.0f)));
    float b = sqrt(2.0f / (5.0f + sqrt(5.0f)));

    vector<Vec3> vertices = // x, y, z
    {
      {-b,  a,  0},
      { b,  a,  0},
      {-b, -a,  0},
      { b, -a,  0},
      { 0, -b,  a},
      { 0,  b,  a},
      { 0, -b, -a},
      { 0,  b, -a},
      { a,  0, -b},
      { a,  0,  b},
      {-a,  0, -b},
      {-a,  0,  b},
    };

    vector<uint32_t> indices =
    {
       0, 11,  5,
       0,  5,  1,
       0,  1,  7,
       0,  7, 10,
       0, 10, 11,
       1,  5,  9,
       5, 11,  4,
      11, 10,  2,
      10,  7,  6,
       7,  1,  8,
       3,  9,  4,
       3,  4,  2,
       3,  2,  6,
       3,  6,  8,
       3,  8,  9,
       4,  9,  5,
       2,  4, 11,
       6,  2, 10,
       8,  6,  7,
       9,  8,  1,
    };

    Icosahedron icosahedron;

    icosahedron.vertices[0] = vertices;
    icosahedron.indices[0] = indices;

    for(int i = 1; i < subdivs; ++i)
    {
      subdivide(vertices, indices);

      icosahedron.vertices[i] = vertices;
      icosahedron.indices[i] = indices;
    }

    icosahedron.subdivs = subdivs;

    return icosahedron;
  }

  float fbm(Perlin3f &perlin, Vec3 const &xyz, float persistence, float lacunarity)
  {
    float sum = 0;
    float amplitude = 1;
    float frequency = 1;

    for(int k = 0; k < 4; ++k)
    {
      sum += perlin(xyz.x * frequency, xyz.y * frequency, xyz.z * frequency).noise * amplitude;

      amplitude *= persistence;
      frequency *= lacunarity;
    }

    return 0.5f + 0.15f * sum;
  }

  Asteroid make_asteroid(ResourceManager &resources, Icosahedron const &base, mt19937 &entropy)
  {
    Asteroid asteroid;

    auto noisescale = 0.5f;
    auto radiusbias = 0.3f;
    auto radiusscale = max(normal_distribution<float>(1.3f, 0.7f)(entropy), 0.2f);
    auto persistence = normal_distribution<float>(0.95f, 0.04f)(entropy);
    auto lacunarity = 2.0f;
    auto perlin = Perlin3f(entropy);

    for(size_t lod = 0; lod < extentof(asteroid.lods); ++lod)
    {
      assert(lod < base.subdivs);

      auto mesh = resources.create<Mesh>(base.vertices[lod].size(), base.indices[lod].size());

      if (auto lump = resources.acquire_lump(mesh->size()))
      {
        auto vertices = lump->memory<Mesh::Vertex>(mesh->verticesoffset());

        for(size_t i = 0; i < base.vertices[lod].size(); ++i)
        {
          auto position = base.vertices[lod][i];

          float radius = (fbm(perlin, position * noisescale, persistence, lacunarity) + radiusbias) * radiusscale;

          vertices[i].position = position * radius;
          vertices[i].texcoord = Vec2(asin(position.x)/pi<float>() + 0.5f, asin(position.y)/pi<float>() + 0.5f) * 5;
        }

        auto indices = lump->memory<uint32_t>(mesh->indicesoffset());

        memcpy(indices, base.indices[lod].data(), base.indices[lod].size()*sizeof(uint32_t));

        calculate_basis(vertices, base.vertices[lod].size(), base.indices[lod].data(), base.indices[lod].size());

        resources.update(mesh, lump, Bound3(Vec3(-1.0, -1.0, 1.0f), Vec3(1.0, 1.0, 1.0f)));

        resources.release_lump(lump);
      }

      asteroid.mass = radiusscale;
      asteroid.lods[lod] = mesh;
    }

    return asteroid;
  }

  void init_thread(DatumPlatform::PlatformInterface &platform, void *ldata, void *rdata)
  {
    auto &state = *static_cast<GameState*>(ldata);

    mt19937 entropy(random_device{}());

    auto icosahedron = make_icosahedron(extent<decltype(Asteroid::lods)>::value);

    for(auto &mesh : state.meshes)
    {
      mesh = make_asteroid(state.resources, icosahedron, entropy);
    }

    normal_distribution<float> orbitradiusdist(SimOrbitRadius, 0.6f * SimDiscRadius);
    normal_distribution<float> heightdist(0.0f, 0.4f);
    uniform_real_distribution<float> angledist(-pi<float>(), pi<float>());
    uniform_real_distribution<float> radialvelocitydist(5.0f, 15.0f);
    uniform_real_distribution<float> spinvelocitydist(-2.0f, 2.0f);
    uniform_real_distribution<float> axisdist(-1.0f, 1.0f);

    for(size_t i = 0; i < extentof(state.instances); ++i)
    {
      auto orbitradius = orbitradiusdist(entropy);
      auto discheight = heightdist(entropy) * SimDiscRadius;
      auto positionangle = angledist(entropy);

      state.instances[i].mesh = i / (extentof(state.instances) / extentof(state.meshes) + 1);
      state.instances[i].material = i / (extentof(state.instances) / extentof(state.materials) + 1);

      auto mass = state.meshes[state.instances[i].mesh].mass;

      state.instances[i].spinaxis = normalise(Vec3(axisdist(entropy), axisdist(entropy), axisdist(entropy)));
      state.instances[i].spinvelocity = spinvelocitydist(entropy) / mass;
      state.instances[i].orbitvelocity = radialvelocitydist(entropy) / (mass * orbitradius);

      state.instances[i].transform = Transform::rotation(Vec3(0, 1, 0), positionangle) * Transform::translation(orbitradius, discheight, 0.0f);
    }

    state.initialised = true;
  }

  struct WorkOrder
  {
    int i;
    int i0, i1;
    float dt;

    atomic<int> *latch;
  };

  void update_thread(DatumPlatform::PlatformInterface &platform, void *ldata, void *rdata)
  {
    auto &work = *static_cast<WorkOrder*>(rdata);
    auto &state = *static_cast<GameState*>(ldata);

    int i0 = work.i0;
    int i1 = work.i1;
    float dt = work.dt;

    for(int i = i0; i < i1; ++i)
    {
      auto &instance = state.instances[i];

      auto orbit = Transform::rotation(Vec3(0, 1, 0), instance.orbitvelocity * dt);
      auto spin = Transform::rotation(instance.spinaxis, instance.spinvelocity * dt);

      instance.transform = orbit * instance.transform * spin;
    }

    auto &cmdlist = state.cmdlists[work.i];

    GeometryList::BuildState buildstate;

    if (cmdlist.begin(buildstate, state.rendercontext, state.resources))
    {
      auto camerapos = state.camera.position();

      for(int i = i0; i < i1; ++i)
      {
        auto &instance = state.instances[i];

        const float MinSubdivSizeLog2 = log2(0.0019f);

        auto position = instance.transform.translation();
        auto distancetoeye = dist(camerapos, position);
        auto relativescreensize = log2(1 / distancetoeye);
        auto lod = clamp(int(relativescreensize - MinSubdivSizeLog2), 0, LodLevels-1);

        auto mesh = state.meshes[instance.mesh].lods[lod];
        auto material = state.materials[instance.material];

        cmdlist.push_mesh(buildstate, instance.transform, mesh, material);
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
  initialised = false;
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

  auto pack = state.assets.load(platform, "asteroids.pack");

  if (!pack)
    throw runtime_error("Data Assets Load Failure");

  state.skybox = state.resources.create<SkyBox>(state.assets.find(pack->id + 1));

  for(size_t i = 0; i < extentof(state.materials); ++i)
  {
    state.materials[i] = state.resources.create<Material>(state.assets.find(pack->id + 2 + i));
  }

  state.camera.lookat(Vec3(-121.4f, 69.9f, 562.8f), Vec3(0, 0, 0), Vec3(0, 1, 0));

  platform.submit_work(init_thread, &state, nullptr);

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
      state.mode = GameState::Init;
    }
  }

  if (state.mode == GameState::Init)
  {
    asset_guard lock(state.assets);

    int ready = 0, total = 0;

    request(platform, state.resources, state.skybox, &ready, &total);

    for(size_t i = 0; i < extentof(state.materials); ++i)
    {
      request(platform, state.resources, state.materials[i], &ready, &total);
    }

    if (state.initialised && ready == total)
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

    float speed = 1.0f;

    if (input.controllers[0].move_up.down())
      state.camera.offset(speed*Vec3(0, 0, -1));

    if (input.controllers[0].move_down.down())
      state.camera.offset(speed*Vec3(0, 0, 1));

    if (input.controllers[0].move_left.down())
      state.camera.offset(speed*Vec3(-1, 0, 0));

    if (input.controllers[0].move_right.down())
      state.camera.offset(speed*Vec3(1, 0, 0));

    state.camera = normalise(state.camera);

    atomic<int> latch(ThreadCount);

    WorkOrder work[ThreadCount];
    for(int i = 0; i < ThreadCount; ++i)
    {
      work[i].i = i;
      work[i].i0 = i * extentof(state.instances) / ThreadCount;
      work[i].i1 = min(work[i].i0 + extentof(state.instances) / ThreadCount, extentof(state.instances));
      work[i].dt = dt;
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

  if (state.mode == GameState::Init)
  {
    RenderList renderlist(platform.renderscratchmemory, 8*1024*1024);

    SpriteList sprites;
    SpriteList::BuildState buildstate;

    if (sprites.begin(buildstate, state.rendercontext, state.resources))
    {
      sprites.push_text(buildstate, Vec2(viewport.width/2 - state.debugfont->width("Initialising...")/2, viewport.height/2 + state.debugfont->height()/2), state.debugfont->height(), state.debugfont, "Initialising...");

      sprites.finalise(buildstate);
    }

    renderlist.push_sprites(sprites);

    render(state.rendercontext, viewport, Camera(), renderlist, renderparams);
  }

  if (state.mode == GameState::Play)
  {
    auto &camera = state.camera;

    RenderList renderlist(platform.renderscratchmemory, 8*1024*1024);

    for(auto &cmdlist : state.cmdlists)
      renderlist.push_geometry(cmdlist);

    renderparams.skybox = state.skybox;

    render(state.rendercontext, viewport, camera, renderlist, renderparams);
  }

  state.resources.release(state.resourcetoken);
}
