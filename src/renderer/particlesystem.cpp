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
  T uniform_distribution(std::mt19937 &entropy, T const &minvalue, T const &maxvalue);

  template<>
  float uniform_distribution<float>(std::mt19937 &entropy, float const &minvalue, float const &maxvalue)
  {
    return uniform_real_distribution<float>{minvalue, maxvalue}(entropy);
  }

  template<>
  Vec2 uniform_distribution<Vec2>(std::mt19937 &entropy, Vec2 const &minvalue, Vec2 const &maxvalue)
  {
    return Vec2(uniform_distribution(entropy, minvalue.x, maxvalue.x), uniform_distribution(entropy, minvalue.y, maxvalue.y));
  }

  template<>
  Vec3 uniform_distribution<Vec3>(std::mt19937 &entropy, Vec3 const &minvalue, Vec3 const &maxvalue)
  {
    return Vec3(uniform_distribution(entropy, minvalue.x, maxvalue.x), uniform_distribution(entropy, minvalue.y, maxvalue.y), uniform_distribution(entropy, minvalue.z, maxvalue.z));
  }

  template<>
  Color3 uniform_distribution<Color3>(std::mt19937 &entropy, Color3 const &minvalue, Color3 const &maxvalue)
  {
    return Color3(uniform_distribution(entropy, minvalue.r, maxvalue.r), uniform_distribution(entropy, minvalue.g, maxvalue.g), uniform_distribution(entropy, minvalue.b, maxvalue.b));
  }

  template<>
  Color4 uniform_distribution<Color4>(std::mt19937 &entropy, Color4 const &minvalue, Color4 const &maxvalue)
  {
    return Color4(uniform_distribution(entropy, minvalue.r, maxvalue.r), uniform_distribution(entropy, minvalue.g, maxvalue.g), uniform_distribution(entropy, minvalue.b, maxvalue.b), uniform_distribution(entropy, minvalue.a, maxvalue.a));
  }

  template<typename T>
  void pack(vector<uint8_t> &bits, T const &value)
  {
    bits.insert(bits.end(), (uint8_t const *)&value, (uint8_t const *)&value + sizeof(value));
  }

  template<typename T, typename U, size_t N>
  void pack(vector<uint8_t> &bits, U const (&values)[N])
  {
    for(auto &value : values)
      pack<T>(bits, value);
  }

  template<typename T>
  void unpack(T &value, void const *bits, size_t &cursor)
  {
    memcpy(&value, (uint8_t const *)bits + cursor, sizeof(value));

    cursor += sizeof(value);
  }

  template<typename U, typename T, enable_if_t<!is_same<T, U>::value>* = nullptr>
  void unpack(T &value, void const *bits, size_t &cursor)
  {
    U tmp;
    memcpy(&tmp, (uint8_t const *)bits + cursor, sizeof(tmp));
    value = static_cast<T>(tmp);

    cursor += sizeof(tmp);
  }

  template<typename T, typename U, size_t N>
  void unpack(U (&values)[N], void const *bits, size_t &cursor)
  {
    for(auto &value : values)
      unpack<T>(value, bits, cursor);
  }
}

//|---------------------- Distribution --------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Distribution::get /////////////////////////////////
template<typename T>
T Distribution<T>::get(std::mt19937 &entropy, float t) const
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

  throw std::logic_error("invalid distribution");
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

    distribution.table[i] = basecolor * fade;
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


//|---------------------- ParticleEmitter -----------------------------------
//|--------------------------------------------------------------------------

vector<uint8_t> pack(ParticleEmitter const &emitter)
{
  vector<uint8_t> bits;

  pack<float>(bits, emitter.duration);
  pack<float>(bits, emitter.rate);
  pack<uint32_t>(bits, emitter.bursts);
  pack<float>(bits, emitter.bursttime);
  pack<uint32_t>(bits, emitter.burstcount);
  pack<uint32_t>(bits, emitter.looping);
  pack<Distribution<float>>(bits, emitter.life);
  pack<Vec2>(bits, emitter.size);
  pack<Distribution<float>>(bits, emitter.scale);
  pack<Distribution<float>>(bits, emitter.rotation);
  pack<Distribution<Vec3>>(bits, emitter.velocity);
  pack<Distribution<Color4>>(bits, emitter.color);
  pack<Distribution<float>>(bits, emitter.layer);
  pack<Vec3>(bits, emitter.acceleration);
  pack<uint32_t>(bits, emitter.modules);

  if (emitter.modules & ParticleEmitter::ShapeEmitter)
  {
    pack<uint32_t>(bits, static_cast<int>(emitter.shape));
    pack<float>(bits, emitter.shaperadius);
    pack<float>(bits, emitter.shapeangle);
  }

  if (emitter.modules & ParticleEmitter::ScaleOverLife)
  {
    pack<Distribution<float>>(bits, emitter.scaleoverlife);
  }

  if (emitter.modules & ParticleEmitter::RotateOverLife)
  {
    pack<Distribution<float>>(bits, emitter.rotateoverlife);
  }

  if (emitter.modules & ParticleEmitter::StretchWithVelocity)
  {
    pack<float>(bits, emitter.velocitystretchmin);
    pack<float>(bits, emitter.velocitystretchmax);
  }

  if (emitter.modules & ParticleEmitter::ColorOverLife)
  {
    pack<Distribution<Color4>>(bits, emitter.coloroverlife);
  }

  if (emitter.modules & ParticleEmitter::LayerOverLife)
  {
    pack<float>(bits, emitter.layerstart);
    pack<float>(bits, emitter.layercount);
    pack<Distribution<float>>(bits, emitter.layerrate);
  }

  return bits;
}


size_t unpack(ParticleEmitter &emitter, void const *bits)
{
  size_t cursor = 0;

  unpack<float>(emitter.duration, bits, cursor);
  unpack<float>(emitter.rate, bits, cursor);
  unpack<uint32_t>(emitter.bursts, bits, cursor);
  unpack<float>(emitter.bursttime, bits, cursor);
  unpack<uint32_t>(emitter.burstcount, bits, cursor);
  unpack<uint32_t>(emitter.looping, bits, cursor);
  unpack<Distribution<float>>(emitter.life, bits, cursor);
  unpack<Vec2>(emitter.size, bits, cursor);
  unpack<Distribution<float>>(emitter.scale, bits, cursor);
  unpack<Distribution<float>>(emitter.rotation, bits, cursor);
  unpack<Distribution<Vec3>>(emitter.velocity, bits, cursor);
  unpack<Distribution<Color4>>(emitter.color, bits, cursor);
  unpack<Distribution<float>>(emitter.layer, bits, cursor);
  unpack<Vec3>(emitter.acceleration, bits, cursor);
  unpack<uint32_t>(emitter.modules, bits, cursor);

  if (emitter.modules & ParticleEmitter::ShapeEmitter)
  {
    unpack<uint32_t>(emitter.shape, bits, cursor);
    unpack<float>(emitter.shaperadius, bits, cursor);
    unpack<float>(emitter.shapeangle, bits, cursor);
  }

  if (emitter.modules & ParticleEmitter::ScaleOverLife)
  {
    unpack<Distribution<float>>(emitter.scaleoverlife, bits, cursor);
  }

  if (emitter.modules & ParticleEmitter::RotateOverLife)
  {
    unpack<Distribution<float>>(emitter.rotateoverlife, bits, cursor);
  }

  if (emitter.modules & ParticleEmitter::StretchWithVelocity)
  {
    unpack<float>(emitter.velocitystretchmin, bits, cursor);
    unpack<float>(emitter.velocitystretchmax, bits, cursor);
  }

  if (emitter.modules & ParticleEmitter::ColorOverLife)
  {
    unpack<Distribution<Color4>>(emitter.coloroverlife, bits, cursor);
  }

  if (emitter.modules & ParticleEmitter::LayerOverLife)
  {
    unpack<float>(emitter.layerstart, bits, cursor);
    unpack<float>(emitter.layercount, bits, cursor);
    unpack<Distribution<float>>(emitter.layerrate, bits, cursor);
  }

  return cursor;
}


//|---------------------- ParticleSystem ------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ParticleSystem::Constructor ///////////////////////
ParticleSystem::ParticleSystem(allocator_type const &allocator)
  : emitters(allocator),
    entropy(random_device{}()),
    m_allocator(allocator)
{
  bound = Bound3(Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));
}


///////////////////////// ParticleSystem::load ///////////////////////////////
bool ParticleSystem::load(DatumPlatform::PlatformInterface &platform, ResourceManager *resources, Asset const *asset)
{
  assert(asset);
  assert(resources->assets()->barriercount != 0);

  auto assets = resources->assets();

  maxparticles = asset->maxparticles;
  bound = Bound3(Vec3(asset->minrange[0], asset->minrange[1], asset->minrange[2]), Vec3(asset->maxrange[0], asset->maxrange[1], asset->maxrange[2]));

  auto bits = assets->request(platform, asset);

  if (!bits)
    return false;

  auto payload = reinterpret_cast<PackParticleSystemPayload const *>(bits);

  spritesheet = resources->create<Texture>(assets->find(asset->id + payload->spritesheet), Texture::Format::SRGBA);

  emitters.resize(asset->emittercount);

  size_t cursor = 0;

  for(size_t i = 0; i < emitters.size(); ++i)
  {
    cursor += unpack(emitters[i], PackParticleSystemPayload::emitter(bits, cursor));
  }

  return true;
}


///////////////////////// ParticleSystem::create ////////////////////////////
ParticleSystem::Instance const *ParticleSystem::create()
{
  size_t bytes = sizeof(InstanceEx) + maxparticles * sizeof(Particle);

  auto instance = new(allocator<char>().allocate(bytes, alignof(InstanceEx))) InstanceEx;

  instance->time = 0;

  instance->count = 0;
  instance->capacity = maxparticles;
  instance->emitter = new(&instance->data + maxparticles * offsetof(Particle, emitter)) size_t[maxparticles];
  instance->life = new(&instance->data + maxparticles * offsetof(Particle, life)) float[maxparticles];
  instance->growth = new(&instance->data + maxparticles * offsetof(Particle, growth)) float[maxparticles];
  instance->position = new(&instance->data + maxparticles * offsetof(Particle, position)) Vec3[maxparticles];
  instance->velocity = new(&instance->data + maxparticles * offsetof(Particle, velocity)) Vec3[maxparticles];
  instance->transform = new(&instance->data + maxparticles * offsetof(Particle, transform)) Matrix2f[maxparticles];
  instance->scale = new(&instance->data + maxparticles * offsetof(Particle, scale)) Vec2[maxparticles];
  instance->rotation = new(&instance->data + maxparticles * offsetof(Particle, rotation)) float[maxparticles];
  instance->color = new(&instance->data + maxparticles * offsetof(Particle, color)) Color4[maxparticles];
  instance->basecolor = new(&instance->data + maxparticles * offsetof(Particle, basecolor)) Color4[maxparticles];
  instance->layer = new(&instance->data + maxparticles * offsetof(Particle, layer)) float[maxparticles];
  instance->layerrate = new(&instance->data + maxparticles * offsetof(Particle, layerrate)) float[maxparticles];

  instance->spritesheet = spritesheet;

  memset(instance->emittime, 0, sizeof(instance->emittime));

  instance->size = bytes;

  return instance;
}


///////////////////////// ParticleSystem::destroy ///////////////////////////
void ParticleSystem::destroy(ParticleSystem::Instance const *instance)
{
  auto instanceex = static_cast<InstanceEx*>(const_cast<Instance*>(instance));

  allocator<char>().deallocate((char*)instanceex, instanceex->size);
}


///////////////////////// ParticleSystem::update ////////////////////////////
void ParticleSystem::update(ParticleSystem::Instance const *instance, Camera const &camera, Transform const &transform, float dt)
{
  assert(emitters.size() < extent<decltype(InstanceEx::emittime)>::value);

  auto instanceex = static_cast<InstanceEx*>(const_cast<Instance*>(instance));

  uniform_real_distribution<float> real01{0.0f, 1.0f};
  uniform_real_distribution<float> real11{-1.0f, 1.0f};

  long modules = 0;
  for(auto const &emitter : emitters)
  {
    modules |= emitter.modules;
  }

  //
  // Emitters
  //

  for(auto const &emitter : emitters)
  {
    float time = emitter.looping ? fmod(instanceex->time + dt, emitter.duration) : instanceex->time + dt;

    if (time < emitter.duration)
    {
      int emitcount = 0;

      if (emitter.rate != 0)
      {
        float &emittime = instanceex->emittime[indexof(emitters, emitter)];

        emittime += dt;
        emitcount = floor(emittime * emitter.rate);
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
        float t = time / (emitter.duration + 1e-6);

        instance->emitter[instance->count] = indexof(emitters, emitter);
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

        instance->position[instance->count] = transform * position;
        instance->velocity[instance->count] = transform.rotation() * direction * emitter.velocity.get(entropy, t);

        instanceex->count += 1;
      }
    }
  }

  //
  // Life
  //

  for(size_t i = 0; i < instance->count; )
  {
    instance->life[i] += instance->growth[i] * dt;

    if (instance->life[i] > 1.0f - 1e-6)
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

  for(size_t i = 0; i < instance->count; ++i)
  {
    instance->velocity[i] += emitters[instance->emitter[i]].acceleration * dt;
  }

  //
  // Position
  //

  for(size_t i = 0; i < instance->count; ++i)
  {
    instance->position[i] += instance->velocity[i] * dt;
  }

  //
  // Transform
  //

  if (modules & (ParticleEmitter::ScaleOverLife | ParticleEmitter::RotateOverLife | ParticleEmitter::StretchWithVelocity | ParticleEmitter::StretchWithAxis))
  {
    auto proj = camera.aspect() * tan(camera.fov()/2);

    for(size_t i = 0; i < instance->count; ++i)
    {
      auto const &emitter = emitters[instance->emitter[i]];

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
    for(size_t i = 0; i < instance->count; ++i)
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
    for(size_t i = 0; i < instance->count; ++i)
    {
      auto const &emitter = emitters[instance->emitter[i]];

      if (emitter.modules & ParticleEmitter::LayerOverLife)
      {
        instance->layer[i] = emitter.layerstart + fmod2(instance->layer[i] + instance->layerrate[i] * dt - emitter.layerstart, emitter.layercount);
      }
    }
  }

  instanceex->spritesheet = spritesheet;

  instanceex->time += dt;
}
