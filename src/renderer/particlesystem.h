//
// Datum - particle system
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "texture.h"
#include "camera.h"
#include "datum/math.h"
#include <random>

//|-------------------- Distribution --------------------------------------
//|------------------------------------------------------------------------

template<typename T>
class Distribution
{
  public:

    enum class Type
    {
      Constant,
      Uniform,
      Table,
      UniformTable
    };

    static constexpr size_t TableSize = 24;

  public:
    Distribution() = default;

    Distribution(T const &value)
      : type(Type::Constant), value(value)
    {
    }

    T get(std::mt19937 &entropy, float t) const;

  public:

    Type type;

    union
    {
      struct // Constant
      {
        T value;
      };

      struct // Uniform
      {
        T minvalue;
        T maxvalue;
      };

      struct // Table
      {
        T table[TableSize];
      };

      struct // Uniform Table
      {
        T mintable[TableSize/2];
        T maxtable[TableSize/2];
      };
    };
};

template<typename T> Distribution<T> make_constant_distribution(T const &value);
template<typename T> Distribution<T> make_uniform_distribution(T const &minvalue, T const &maxvalue);
template<typename T> Distribution<T> make_table_distribution(T const *values, size_t n);
template<typename T> Distribution<T> make_uniformtable_distribution(T const *minvalues, size_t m, T const *maxvalues, size_t n);

Distribution<lml::Color4> make_colorfade_distribution(lml::Color4 const &basecolor, float startfade = 0.90f);


//|-------------------- ParticleEmitter -----------------------------------
//|------------------------------------------------------------------------

class ParticleEmitter
{
  public:

    using Vec2 = lml::Vec2;
    using Vec3 = lml::Vec3;
    using Color3 = lml::Color3;
    using Color4 = lml::Color4;
    using Transform = lml::Transform;

    enum Modules
    {
      ShapeEmitter = 0x01,
      ScaleOverLife = 0x02,
      RotateOverLife = 0x04,
      ColorOverLife = 0x08,
      LayerOverLife = 0x10,
      StretchWithVelocity = 0x20,
      StretchWithAxis = 0x40,
    };

  public:

    float duration = 2.0f;

    bool looping = true;

    Transform transform = Transform::identity();

    float rate = 10.0f;

    int bursts = 0;
    float bursttime[8];
    int burstcount[8];

    Distribution<float> life = 2.0f;
    Vec2 size = Vec2(1.0f, 1.0f);
    Distribution<float> scale = 1.0f;
    Distribution<float> rotation = 0.0f;
    Distribution<Vec3> velocity = Vec3(8.0f, 0.0f, 0.0f);
    Distribution<Color4> color = Color4(1.0f, 1.0f, 1.0f, 1.0f);
    Distribution<float> emissive = 0.0f;
    Distribution<float> layer = 0.0f;

    Vec3 acceleration = Vec3(0.0f, -9.81f, 0.0f);

    long modules = 0;

    // ShapeEmitter
    enum class Shape { Sphere, Hemisphere, Cone } shape;
    float shaperadius = 1.0f;
    float shapeangle = 0.0f;

    // ScaleOverLife
    Distribution<float> scaleoverlife;

    // RotateOverLife
    Distribution<float> rotateoverlife;

    // ColorOverLife
    Distribution<Color4> coloroverlife;

    // LayerOverLife
    float layerstart = 0.0f;
    float layercount = 1.0f;
    Distribution<float> layerrate = 0.0f;

    // StretchWithVelocity
    float velocitystretchmin = 1.0f;
    float velocitystretchmax = 5.0f;

    // StretchWithAxis
    Vec3 stretchaxis = Vec3(0, 1, 0);
};

void pack(std::vector<uint8_t> &bits, ParticleEmitter const &emitter);
void unpack(ParticleEmitter &emitter, void const *bits, size_t &cursor);


//|-------------------- ParticleSystem ------------------------------------
//|------------------------------------------------------------------------

class ParticleSystem
{
  public:

    struct Instance
    {
      int count;
      int capacity;

      size_t *emitter;
      float *life;
      float *growth;
      lml::Vec3 *position;
      lml::Vec3 *velocity;
      lml::Matrix2f *transform;
      lml::Vec2 *scale;
      float *rotation;
      lml::Color4 *color;
      lml::Color4 *basecolor;
      float *emissive;
      float *layer;
      float *layerrate;
    };

  public:
    friend ParticleSystem const *ResourceManager::create<ParticleSystem>(Asset const *asset);
    friend ParticleSystem const *ResourceManager::create<ParticleSystem>(int maxparticles, lml::Bound3 bound, Texture const *spritesheet, size_t emittercount, ParticleEmitter *emitters);

    bool ready() const { return (state == State::Ready); }

  public:

    int flags;

    lml::Bound3 bound;

    int maxparticles = 1000;

    Texture const *spritesheet;

    size_t emittercount;
    ParticleEmitter const *emitters;

    mutable std::mt19937 entropy;

  public:

    Instance *create(StackAllocator<> const &allocator) const;
    Instance *create(StackAllocatorWithFreelist<> const &allocator) const;

    void update(Instance *instance, Camera const &camera, lml::Transform const &transform, float dt) const;

    void destroy(Instance *instance) const;

  public:

    enum class State
    {
      Empty,
      Loading,
      Waiting,
      Testing,
      Ready,
    };

    Asset const *asset;

    std::atomic<State> state;

    alignas(16) uint8_t data[1];

  private:

    struct Particle
    {
      size_t emitter[1];
      float life[1];
      float growth[1];
      lml::Vec3 position[1];
      lml::Vec3 velocity[1];
      lml::Matrix2f transform[1];
      lml::Vec2 scale[1];
      float rotation[1];
      lml::Color4 color[1];
      lml::Color4 basecolor[1];
      float emissive[1];
      float layer[1];
      float layerrate[1];
    };

    struct InstanceEx : public Instance
    {
      InstanceEx(int maxparticles);

      size_t size;
      FreeList *freelist;

      float time[16];
      float emittime[16];

      alignas(16) uint8_t data[1];
    };

  protected:
    ParticleSystem();
};
