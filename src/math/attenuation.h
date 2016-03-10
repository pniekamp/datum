//
// Datum - coattenuationlor
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "leap/lml/vector.h"

namespace lml
{
  using namespace leap::lml;


  //|-------------------- Attenuation ---------------------------------------
  //|------------------------------------------------------------------------

  class Attenuation : public VectorView<Attenuation, float, 0, 1, 2>
  {
    public:
      Attenuation() = default;
      constexpr Attenuation(float exponent, float linear, float constant);

    union
    {
      struct
      {
        float exponent;
        float linear;
        float constant;
      };
    };
  };


  //|///////////////////// Attenuation::Constructor /////////////////////////
  constexpr Attenuation::Attenuation(float exponent, float linear, float constant)
    : exponent(exponent), linear(linear), constant(constant)
  {
  }


  //|///////////////////// Attenuation range ////////////////////////////////
  inline float range(Attenuation const &attenuation, float intensity)
  {
    auto exponent = attenuation.exponent;
    auto linear = attenuation.linear;
    auto constant = attenuation.constant;

    return (-linear + sqrt(linear*linear - 4*exponent*(constant - 256*intensity))) / (2*exponent);
  }
}
