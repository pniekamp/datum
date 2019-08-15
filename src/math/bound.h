//
// Datum - bound
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "vec.h"
#include <leap/lml/bound.h>
#include "transform.h"

namespace lml
{
  using namespace leap::lml;

  //|---------------------- Bound3 ------------------------------------------
  //|------------------------------------------------------------------------

  class Bound3 : public BoundView<Bound3, float, 3, 0, 1, 2>
  {
    public:
      Bound3() = default;
      constexpr Bound3(Vec3 const &min, Vec3 const &max);

      Vec3 centre() const { return (min + max)/2; }
      Vec3 halfdim() const { return (max - min)/2; }

      Vec3 min;
      Vec3 max;
  };


  //////////////////////// Bound3::Constructor //////////////////////////////
  constexpr Bound3::Bound3(Vec3 const &min, Vec3 const &max)
    : min(min), max(max)
  {
  }


  //////////////////////// Bound3 stream << /////////////////////////////////
  inline std::ostream &operator <<(std::ostream &os, Bound3 const &bound)
  {
    os << "[Bound:" << bound.min << "," << bound.max << "]";

    return os;
  }


  //////////////////////// transform ////////////////////////////////////////
  /// transform bound
  inline Bound3 operator *(Transform const &transform, Bound3 const &bound)
  {
    auto centre = transform * bound.centre();

    auto halfdim = bound.halfdim();
    auto rx = dot(abs(transform.rotation().xaxis()), halfdim);
    auto ry = dot(abs(transform.rotation().yaxis()), halfdim);
    auto rz = dot(abs(transform.rotation().zaxis()), halfdim);

    return Bound3(centre - Vec3(rx, ry, rz), centre + Vec3(rx, ry, rz));
  }

}
