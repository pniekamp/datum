//
// Datum - particle system
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "particlesystem.h"
#include "assetpack.h"
#include <leap/lml/matrixconstants.h>
#include "debug.h"

using namespace std;
using namespace lml;
using leap::indexof;
using leap::extentof;

enum ParticleSystemFlags
{
  ParticleSystemOwnsSpritesheet = 0x01,
};

namespace
{
  size_t particlesystem_datasize(int emittercount)
  {
    return emittercount * sizeof(ParticleEmitter);
  }
}

namespace
{
  template<typename T>
  T table_lookup(T const *table, size_t n, float t)
  {
    assert(t >= 0.0f && t < 1.0f && t * (n-1) < n-1);

    float k;
    float mu = modf(t * (n - 1), &k);

    return table[(size_t)k]*(1-mu) + table[(size_t)k+1]*(mu);
  }

  template<typename T>
  T uniform_distribution(mt19937 &entropy, T const &minvalue, T const &maxvalue);

  template<>
  float uniform_distribution<float>(mt19937 &entropy, float const &minvalue, float const &maxvalue)
  {
    return uniform_real_distribution<float>{minvalue, maxvalue}(entropy);
  }

  template<>
  Vec2 uniform_distribution<Vec2>(mt19937 &entropy, Vec2 const &minvalue, Vec2 const &maxvalue)
  {
    return Vec2(uniform_distribution(entropy, minvalue.x, maxvalue.x), uniform_distribution(entropy, minvalue.y, maxvalue.y));
  }

  template<>
  Vec3 uniform_distribution<Vec3>(mt19937 &entropy, Vec3 const &minvalue, Vec3 const &maxvalue)
  {
    return Vec3(uniform_distribution(entropy, minvalue.x, maxvalue.x), uniform_distribution(entropy, minvalue.y, maxvalue.y), uniform_distribution(entropy, minvalue.z, maxvalue.z));
  }

  template<>
  Color3 uniform_distribution<Color3>(mt19937 &entropy, Color3 const &minvalue, Color3 const &maxvalue)
  {
    return Color3(uniform_distribution(entropy, minvalue.r, maxvalue.r), uniform_distribution(entropy, minvalue.g, maxvalue.g), uniform_distribution(entropy, minvalue.b, maxvalue.b));
  }

  template<>
  Color4 uniform_distribution<Color4>(mt19937 &entropy, Color4 const &minvalue, Color4 const &maxvalue)
  {
    return Color4(uniform_distribution(entropy, minvalue.r, maxvalue.r), uniform_distribution(entropy, minvalue.g, maxvalue.g), uniform_distribution(entropy, minvalue.b, maxvalue.b), uniform_distribution(entropy, minvalue.a, maxvalue.a));
  }
}

//|---------------------- Distribution --------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Distribution::get /////////////////////////////////
template<typename T>
T Distribution<T>::get(mt19937 &entropy, float t) const
{
  switch(type)
  {
    case Type::Constant:
      return value;

    case Type::Uniform:
      return uniform_distribution(entropy, minvalue, maxvalue);

    case Type::Table:
      return table_lookup(table, extentof(table), t);

    case Type::UniformTable:
      return uniform_distribution(entropy, table_lookup(mintable, extentof(mintable), t), table_lookup(maxtable, extentof(maxtable), t));
  }

  throw logic_error("invalid distribution");
}

///////////////////////// make_constant_distribution ////////////////////////
template<typename T>
Distribution<T> make_constant_distribution(T const &value)
{
  return value;
}

///////////////////////// make_uniform_distribution /////////////////////////
template<typename T>
Distribution<T> make_uniform_distribution(T const &minvalue, T const &maxvalue)
{
  Distribution<T> distribution;

  distribution.type = Distribution<T>::Type::Uniform;
  distribution.minvalue = minvalue;
  distribution.maxvalue = maxvalue;

  return distribution;
}

///////////////////////// make_table_distribution ///////////////////////////
template<typename T>
Distribution<T> make_table_distribution(T const *values, size_t n)
{
  assert(n == Distribution<T>::TableSize);

  Distribution<T> distribution;

  distribution.type = Distribution<T>::Type::Table;

  memcpy(distribution.table, values, sizeof(distribution.table));

  return distribution;
}

///////////////////////// make_uniformtable_distribution ////////////////////
template<typename T>
Distribution<T> make_uniformtable_distribution(T const *minvalues, size_t m, T const *maxvalues, size_t n)
{
  assert(m == Distribution<T>::TableSize/2);
  assert(n == Distribution<T>::TableSize/2);

  Distribution<T> distribution;

  distribution.type = Distribution<T>::Type::UniformTable;

  memcpy(distribution.mintable, minvalues, sizeof(distribution.mintable));
  memcpy(distribution.maxtable, maxvalues, sizeof(distribution.maxtable));

  return distribution;
}

///////////////////////// make_colorfade_distribution ///////////////////////
Distribution<lml::Color4> make_colorfade_distribution(Color4 const &basecolor, float startfade)
{
  Distribution<Color4> distribution;

  distribution.type = Distribution<Color4>::Type::Table;

  for(size_t i = 0; i < extentof(distribution.table); ++i)
  {
    auto fade = 1.0f - clamp(i / ((1-startfade) * (extentof(distribution.table) - 2)) - startfade/(1-startfade), 0.0f, 1.0f);

    distribution.table[i] = Color4(basecolor.rgb, basecolor.a * fade);
  }

  return distribution;
}

// Explicit Instantiations
template class Distribution<float>;
template class Distribution<Vec2>;
template class Distribution<Vec3>;
template class Distribution<Color3>;
template class Distribution<Color4>;
template Distribution<float> make_constant_distribution<float>(float const &value);
template Distribution<Vec2> make_constant_distribution<Vec2>(Vec2 const &value);
template Distribution<Vec3> make_constant_distribution<Vec3>(Vec3 const &value);
template Distribution<Color3> make_constant_distribution<Color3>(Color3 const &value);
template Distribution<Color4> make_constant_distribution<Color4>(Color4 const &value);
template Distribution<float> make_uniform_distribution<float>(float const &minvalue, float const &maxvalue);
template Distribution<Vec2> make_uniform_distribution<Vec2>(Vec2 const &minvalue, Vec2 const &maxvalue);
template Distribution<Vec3> make_uniform_distribution<Vec3>(Vec3 const &minvalue, Vec3 const &maxvalue);
template Distribution<Color3> make_uniform_distribution<Color3>(Color3 const &minvalue, Color3 const &maxvalue);
template Distribution<Color4> make_uniform_distribution<Color4>(Color4 const &minvalue, Color4 const &maxvalue);
template Distribution<float> make_table_distribution<float>(float const *values, size_t n);
template Distribution<Vec2> make_table_distribution<Vec2>(Vec2 const *values, size_t n);
template Distribution<Vec3> make_table_distribution<Vec3>(Vec3 const *values, size_t n);
template Distribution<Color3> make_table_distribution<Color3>(Color3 const *values, size_t n);
template Distribution<Color4> make_table_distribution<Color4>(Color4 const *values, size_t n);
template Distribution<float> make_uniformtable_distribution<float>(float const *minvalues, size_t m, float const *maxvalues, size_t n);
template Distribution<Vec2> make_uniformtable_distribution<Vec2>(Vec2 const *minvalues, size_t m, Vec2 const *maxvalues, size_t n);
template Distribution<Vec3> make_uniformtable_distribution<Vec3>(Vec3 const *minvalues, size_t m, Vec3 const *maxvalues, size_t n);
template Distribution<Color3> make_uniformtable_distribution<Color3>(Color3 const *minvalues, size_t m, Color3 const *maxvalues, size_t n);
template Distribution<Color4> make_uniformtable_distribution<Color4>(Color4 const *minvalues, size_t m, Color4 const *maxvalues, size_t n);


//|---------------------- Instance ------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Instance::Constructor /////////////////////////////
ParticleSystem::InstanceEx::InstanceEx(int maxparticles)
{
  count = 0;
  capacity = maxparticles;
  emitter = new(&data + capacity * offsetof(Particle, emitter)) size_t[capacity];
  life = new(&data + capacity * offsetof(Particle, life)) float[capacity];
  growth = new(&data + capacity * offsetof(Particle, growth)) float[capacity];
  position = new(&data + capacity * offsetof(Particle, position)) Vec3[capacity];
  velocity = new(&data + capacity * offsetof(Particle, velocity)) Vec3[capacity];
  transform = new(&data + capacity * offsetof(Particle, transform)) Matrix2f[capacity];
  scale = new(&data + capacity * offsetof(Particle, scale)) Vec2[capacity];
  rotation = new(&data + capacity * offsetof(Particle, rotation)) float[capacity];
  color = new(&data + capacity * offsetof(Particle, color)) Color4[capacity];
  basecolor = new(&data + capacity * offsetof(Particle, basecolor)) Color4[capacity];
  layer = new(&data + capacity * offsetof(Particle, layer)) float[capacity];
  layerrate = new(&data + capacity * offsetof(Particle, layerrate)) float[capacity];

  memset(time, 0, sizeof(time));
  memset(emittime, 0, sizeof(emittime));
}


//|---------------------- ParticleSystem ------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ParticleSystem::Constructor ///////////////////////
ParticleSystem::ParticleSystem()
  : entropy(random_device{}())
{
}


///////////////////////// ParticleSystem::create ////////////////////////////
ParticleSystem::Instance *ParticleSystem::create(StackAllocator<> const &allocator) const
{
  size_t bytes = sizeof(InstanceEx) + maxparticles * sizeof(Particle);

  auto instance = new(allocate<char>(allocator, bytes, alignof(InstanceEx))) InstanceEx(maxparticles);

  instance->size = bytes;
  instance->freelist = nullptr;

  return instance;
}

ParticleSystem::Instance *ParticleSystem::create(StackAllocatorWithFreelist<> const &allocator) const
{
  size_t bytes = sizeof(InstanceEx) + maxparticles * sizeof(Particle);

  auto instance = new(allocate<char>(allocator, bytes, alignof(InstanceEx))) InstanceEx(maxparticles);

  instance->size = bytes;
  instance->freelist = &allocator.freelist();

  return instance;
}


///////////////////////// ParticleSystem::destroy ///////////////////////////
void ParticleSystem::destroy(Instance *instance) const
{
  auto instanceex = static_cast<InstanceEx*>(const_cast<Instance*>(instance));

  if (instanceex->freelist)
  {
    instanceex->freelist->release(instanceex, instanceex->size);
  }
}


///////////////////////// ParticleSystem::update ////////////////////////////
void ParticleSystem::update(ParticleSystem::Instance *instance, Camera const &camera, Transform const &transform, float dt) const
{
  assert(ready());
  assert(instance);
  assert(emittercount < extent<decltype(InstanceEx::time)>::value);
  assert(emittercount < extent<decltype(InstanceEx::emittime)>::value);

  auto instanceex = static_cast<InstanceEx*>(instance);

  uniform_real_distribution<float> real01{0.0f, 1.0f};
  uniform_real_distribution<float> real11{-1.0f, 1.0f};

  long modules = 0;
  for(size_t k = 0; k < emittercount; ++k)
  {
    modules |= emitters[k].modules;
  }

  //
  // Emitters
  //

  for(size_t k = 0; k < emittercount; ++k)
  {
    auto &emitter = emitters[k];

    auto &time = instanceex->time[k];

    if (time < emitter.duration)
    {
      int emitcount = 0;

      if (emitter.rate != 0)
      {
        auto &emittime = instanceex->emittime[k];

        emittime += dt;
        emitcount = static_cast<int>(emittime * emitter.rate);
        emittime = emittime - emitcount / emitter.rate;
      }

      for(int i = 0; i < emitter.bursts; ++i)
      {
        if (time - dt <= emitter.bursttime[i] && emitter.bursttime[i] < time)
        {
          emitcount += emitter.burstcount[i];
        }
      }

      for(int i = 0; i < emitcount && instance->count < instance->capacity; ++i)
      {
        auto t = time / (emitter.duration + 1e-6f);

        instance->emitter[instance->count] = k;
        instance->life[instance->count] = 0.0f;
        instance->growth[instance->count] = 1.0f / emitter.life.get(entropy, t);
        instance->scale[instance->count] = emitter.size * emitter.scale.get(entropy, t);
        instance->rotation[instance->count] = emitter.rotation.get(entropy, t);
        instance->transform[instance->count] = RotationMatrix(instance->rotation[instance->count]) * ScaleMatrix(instance->scale[instance->count]);
        instance->basecolor[instance->count] = emitter.color.get(entropy, t);
        instance->color[instance->count] = instance->basecolor[instance->count];
        instance->layer[instance->count] = emitter.layer.get(entropy, t);
        instance->layerrate[instance->count] = emitter.layerrate.get(entropy, t);

        if (emitter.layerrate.type == Distribution<float>::Type::Constant && emitter.layerrate.value == 0.0f)
        {
          instance->layerrate[instance->count] = emitter.layercount * instance->growth[instance->count];
        }

        auto position = Vec3(0.0f, 0.0f, 0.0f);
        auto direction = Quaternion3f(1.0f, 0.0f, 0.0f, 0.0f);

        if (emitter.modules & ParticleEmitter::ShapeEmitter)
        {
          switch (emitter.shape)
          {
            case ParticleEmitter::Shape::Sphere:
            {
              auto radius2 = emitter.shaperadius * emitter.shaperadius;

              for(int i = 0; i < 8; ++i)
              {
                Vec3 location = Vec3(real11(entropy), real11(entropy), real11(entropy)) * emitter.shaperadius;

                if (normsqr(location) < radius2)
                {
                  position = location;
                  direction = Quaternion3f(zUnit3f, theta(location)) * Quaternion3f(yUnit3f, phi(location) - pi<float>()/2);
                  break;
                }
              }

              break;
            }

            case ParticleEmitter::Shape::Hemisphere:
            {
              auto radius2 = emitter.shaperadius * emitter.shaperadius;

              for(int i = 0; i < 8; ++i)
              {
                Vec3 location = Vec3(real01(entropy), real11(entropy), real11(entropy)) * emitter.shaperadius;

                if (normsqr(location) < radius2)
                {
                  position = location;
                  direction = Quaternion3f(zUnit3f, theta(location)) * Quaternion3f(yUnit3f, phi(location) - pi<float>()/2);
                  break;
                }
              }

              break;
            }

            case ParticleEmitter::Shape::Cone:
            {
              auto radius2 = emitter.shaperadius * emitter.shaperadius;

              for(int i = 0; i < 8; ++i)
              {
                Vec3 location = Vec3(0.0f, real11(entropy), real11(entropy)) * emitter.shaperadius;

                if (normsqr(location) < radius2)
                {
                  position = location;
                  direction = Quaternion3f(xUnit3f, atan2(location.y, -location.z)) * Quaternion3f(yUnit3f, emitter.shapeangle * norm(location) / emitter.shaperadius);
                  break;
                }
              }

              break;
            }
          }
        }

        instance->position[instance->count] = transform * emitter.transform * position;
        instance->velocity[instance->count] = transform.rotation() * emitter.transform.rotation() * direction * emitter.velocity.get(entropy, t);

        instanceex->count += 1;
      }

      time = emitter.looping ? fmod(time + dt, emitter.duration) : time + dt;
    }
  }

  //
  // Life
  //

  for(int i = 0; i < instance->count; )
  {
    instance->life[i] += instance->growth[i] * dt;

    if (instance->life[i] > 1.0f - 1e-6 || instance->emitter[i] >= emittercount)
    {
      instance->emitter[i] = instance->emitter[instance->count-1];
      instance->life[i] = instance->life[instance->count-1];
      instance->growth[i] = instance->growth[instance->count-1];
      instance->position[i] = instance->position[instance->count-1];
      instance->velocity[i] = instance->velocity[instance->count-1];
      instance->transform[i] = instance->transform[instance->count-1];
      instance->scale[i] = instance->scale[instance->count-1];
      instance->rotation[i] = instance->rotation[instance->count-1];
      instance->color[i] = instance->color[instance->count-1];
      instance->basecolor[i] = instance->basecolor[instance->count-1];
      instance->layer[i] = instance->layer[instance->count-1];
      instance->layerrate[i] = instance->layerrate[instance->count-1];

      instanceex->count -= 1;
    }
    else
      ++i;
  }

  //
  // Velocity
  //

  for(int i = 0; i < instance->count; ++i)
  {
    instance->velocity[i] += emitters[instance->emitter[i]].acceleration * dt;
  }

  //
  // Position
  //

  for(int i = 0; i < instance->count; ++i)
  {
    instance->position[i] += instance->velocity[i] * dt;
  }

  //
  // Transform
  //

  if (modules & (ParticleEmitter::ScaleOverLife | ParticleEmitter::RotateOverLife | ParticleEmitter::StretchWithVelocity | ParticleEmitter::StretchWithAxis))
  {
    auto proj = camera.aspect() * tan(camera.fov()/2);

    for(int i = 0; i < instance->count; ++i)
    {
      auto &emitter = emitters[instance->emitter[i]];

      auto scale = instance->scale[i];
      auto rotation = instance->rotation[i];

      if (emitter.modules & ParticleEmitter::ScaleOverLife)
      {
        scale *= emitter.scaleoverlife.get(entropy, instance->life[i]);
      }

      if (emitter.modules & ParticleEmitter::RotateOverLife)
      {
        rotation += emitter.rotateoverlife.get(entropy, instance->life[i]);
      }

      if (emitter.modules & (ParticleEmitter::ScaleOverLife | ParticleEmitter::RotateOverLife | ParticleEmitter::StretchWithVelocity | ParticleEmitter::StretchWithAxis))
      {
        instance->transform[i] = RotationMatrix(rotation) * ScaleMatrix(scale);
      }

      if (emitter.modules & ParticleEmitter::StretchWithVelocity)
      {
        auto pos = inverse(camera.transform()) * instance->position[i];
        auto angle = Quaternion3f(yUnit3f, proj * (-pos.x / pos.z)) * Quaternion3f(xUnit3f, proj * (pos.y / pos.z)) * conjugate(camera.rotation()) * instance->velocity[i];
        auto stretch = rotatey(Vec3(1.0f, 1.0f, clamp(norm(angle), emitter.velocitystretchmin, emitter.velocitystretchmax)), phi(abs(angle)));

        instance->transform[i] = RotationMatrix(theta(angle)) * ScaleMatrix(stretch.xy) * instance->transform[i];
      }

      if (emitter.modules & ParticleEmitter::StretchWithAxis)
      {
        auto pos = inverse(camera.transform()) * instance->position[i];
        auto angle = Quaternion3f(yUnit3f, proj * (-pos.x / pos.z)) * Quaternion3f(xUnit3f, proj * (pos.y / pos.z)) * conjugate(camera.rotation()) * emitter.stretchaxis;
        auto stretch = rotatey(Vec3(1.0f, 1.0f, 0.0f), phi(abs(angle)));

        instance->transform[i] = RotationMatrix(theta(angle)) * ScaleMatrix(stretch.xy) * instance->transform[i];
      }
    }
  }

  //
  // Color
  //

  if (modules & ParticleEmitter::ColorOverLife)
  {
    for(int i = 0; i < instance->count; ++i)
    {
      auto const &emitter = emitters[instance->emitter[i]];

      if (emitter.modules & ParticleEmitter::ColorOverLife)
      {
        instance->color[i] = hada(instance->basecolor[i], emitter.coloroverlife.get(entropy, instance->life[i]));
      }
    }
  }

  //
  // Layer
  //

  if (modules & ParticleEmitter::LayerOverLife)
  {
    for(int i = 0; i < instance->count; ++i)
    {
      auto const &emitter = emitters[instance->emitter[i]];

      if (emitter.modules & ParticleEmitter::LayerOverLife)
      {
        instance->layer[i] = emitter.layerstart + fmod2(instance->layer[i] + instance->layerrate[i] * dt - emitter.layerstart, emitter.layercount);
      }
    }
  }
}


//|---------------------- ParticleSystem ------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
ParticleSystem const *ResourceManager::create<ParticleSystem>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(ParticleSystem) + particlesystem_datasize(asset->emittercount));

  if (!slot)
    return nullptr;

  auto particlesystem = new(slot) ParticleSystem;

  particlesystem->flags = 0;

  particlesystem->maxparticles = asset->maxparticles;
  particlesystem->bound = Bound3(Vec3(asset->minrange[0], asset->minrange[1], asset->minrange[2]), Vec3(asset->maxrange[0], asset->maxrange[1], asset->maxrange[2]));
  particlesystem->spritesheet = nullptr;
  particlesystem->emittercount = asset->emittercount;
  particlesystem->emitters = nullptr;
  particlesystem->asset = asset;
  particlesystem->state = ParticleSystem::State::Empty;

  return particlesystem;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
ParticleSystem const *ResourceManager::create<ParticleSystem>(int maxparticles, Bound3 bound, Texture const *spritesheet, size_t emittercount, ParticleEmitter *emitters)
{
  auto slot = acquire_slot(sizeof(ParticleSystem) + particlesystem_datasize(emittercount));

  if (!slot)
    return nullptr;

  auto particlesystem = new(slot) ParticleSystem;

  particlesystem->flags = 0;

  particlesystem->maxparticles = maxparticles;
  particlesystem->bound = bound;
  particlesystem->spritesheet = spritesheet;
  particlesystem->emittercount = emittercount;
  particlesystem->emitters = nullptr;
  particlesystem->asset = nullptr;
  particlesystem->state = ParticleSystem::State::Waiting;

  if (emittercount != 0)
  {
    auto emittersdata = reinterpret_cast<ParticleEmitter*>(particlesystem->data);

    copy(emitters, emitters + emittercount, emittersdata);

    particlesystem->emitters = emittersdata;
  }

  if (spritesheet && spritesheet->ready())
    particlesystem->state = ParticleSystem::State::Ready;

  return particlesystem;
}

template<>
ParticleSystem const *ResourceManager::create<ParticleSystem>(int maxparticles, Bound3 bound, Texture const *spritesheet, int emittercount, ParticleEmitter *emitters)
{
  return create<ParticleSystem>(maxparticles, bound, spritesheet, (size_t)emittercount, emitters);
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<ParticleSystem>(DatumPlatform::PlatformInterface &platform, ParticleSystem const *particlesystem)
{
  assert(particlesystem);

  auto slot = const_cast<ParticleSystem*>(particlesystem);

  ParticleSystem::State empty = ParticleSystem::State::Empty;

  if (slot->state.compare_exchange_strong(empty, ParticleSystem::State::Loading))
  {
    if (auto asset = slot->asset)
    {
      assert(assets()->barriercount != 0);

      if (auto bits = m_assets->request(platform, asset))
      {
        auto payload = reinterpret_cast<PackParticleSystemPayload const *>(bits);

        auto emittersdata = reinterpret_cast<ParticleEmitter*>(slot->data);

        size_t cursor = sizeof(PackParticleSystemPayload);

        for(size_t i = 0; i < asset->emittercount; ++i)
        {
          unpack(emittersdata[i], bits, cursor);
        }

        slot->emitters = emittersdata;

        slot->spritesheet = create<Texture>(assets()->find(asset->id + payload->spritesheet), Texture::Format::SRGBA);

        slot->flags |= ParticleSystemOwnsSpritesheet;
      }
    }

    slot->state = (slot->emitters && slot->spritesheet) ? ParticleSystem::State::Waiting : ParticleSystem::State::Empty;
  }

  ParticleSystem::State waiting = ParticleSystem::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, ParticleSystem::State::Testing))
  {
    request(platform, slot->spritesheet);

    slot->state = (slot->spritesheet->ready()) ? ParticleSystem::State::Ready : ParticleSystem::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<ParticleSystem>(ParticleSystem const *particlesystem)
{
  defer_destroy(particlesystem);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<ParticleSystem>(ParticleSystem const *particlesystem)
{
  if (particlesystem)
  {
    if (particlesystem->flags & ParticleSystemOwnsSpritesheet)
      destroy(particlesystem->spritesheet);

    particlesystem->~ParticleSystem();

    release_slot(const_cast<ParticleSystem*>(particlesystem), sizeof(ParticleSystem) + particlesystem_datasize(particlesystem->emittercount));
  }
}
