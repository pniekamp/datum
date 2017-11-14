//
// Datum - asset pack
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "assetpack.h"
#include "renderer/particlesystem.h"
#include "debug.h"

using namespace std;
using namespace lml;
using leap::indexof;
using leap::extentof;

namespace
{
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



//|---------------------- ParticleEmitter -----------------------------------
//|--------------------------------------------------------------------------

///////////////////////// pack //////////////////////////////////////////////
void pack(vector<uint8_t> &bits, ParticleEmitter const &emitter)
{
  pack<float>(bits, emitter.duration);
  pack<uint32_t>(bits, emitter.looping);
  pack<Transform>(bits, emitter.transform);
  pack<float>(bits, emitter.rate);
  pack<uint32_t>(bits, emitter.bursts);
  pack<float>(bits, emitter.bursttime);
  pack<uint32_t>(bits, emitter.burstcount);
  pack<Distribution<float>>(bits, emitter.life);
  pack<Vec2>(bits, emitter.size);
  pack<Distribution<float>>(bits, emitter.scale);
  pack<Distribution<float>>(bits, emitter.rotation);
  pack<Distribution<Vec3>>(bits, emitter.velocity);
  pack<Distribution<Color4>>(bits, emitter.color);
  pack<Distribution<float>>(bits, emitter.emissive);
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
}


///////////////////////// unpack ////////////////////////////////////////////
void unpack(ParticleEmitter &emitter, void const *bits, size_t &cursor)
{
  unpack<float>(emitter.duration, bits, cursor);
  unpack<uint32_t>(emitter.looping, bits, cursor);
  unpack<Transform>(emitter.transform, bits, cursor);
  unpack<float>(emitter.rate, bits, cursor);
  unpack<uint32_t>(emitter.bursts, bits, cursor);
  unpack<float>(emitter.bursttime, bits, cursor);
  unpack<uint32_t>(emitter.burstcount, bits, cursor);
  unpack<Distribution<float>>(emitter.life, bits, cursor);
  unpack<Vec2>(emitter.size, bits, cursor);
  unpack<Distribution<float>>(emitter.scale, bits, cursor);
  unpack<Distribution<float>>(emitter.rotation, bits, cursor);
  unpack<Distribution<Vec3>>(emitter.velocity, bits, cursor);
  unpack<Distribution<Color4>>(emitter.color, bits, cursor);
  unpack<Distribution<float>>(emitter.emissive, bits, cursor);
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
}
