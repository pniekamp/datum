//
// Datum - rect
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "vec.h"
#include "leap/lml/bound.h"

namespace lml
{
  using namespace leap::lml;


  //|-------------------- Rect2 ---------------------------------------------
  //|------------------------------------------------------------------------

  class Rect2 : public BoundView<Rect2, float, 2, 0, 1>
  {
    public:
      Rect2() = default;
      constexpr Rect2(Vec2 const &min, Vec2 const &max);

      Vec2 centre() const { return (min + max)/2; }
      Vec2 halfdim() const { return (max - min)/2; }

      Vec2 min;
      Vec2 max;
  };


  //|///////////////////// Rect2::Constructor ///////////////////////////////
  constexpr Rect2::Rect2(Vec2 const &min, Vec2 const &max)
    : min(min), max(max)
  {
  }


  //|-------------------- Rect3 ---------------------------------------------
  //|------------------------------------------------------------------------

  class Rect3 : public BoundView<Rect3, float, 3, 0, 1, 2>
  {
    public:
      Rect3() = default;
      constexpr Rect3(Vec3 const &min, Vec3 const &max);

      Vec3 centre() const { return (min + max)/2; }
      Vec3 halfdim() const { return (max - min)/2; }

      Vec3 min;
      Vec3 max;
  };


  //|///////////////////// Rect3::Constructor ///////////////////////////////
  constexpr Rect3::Rect3(Vec3 const &min, Vec3 const &max)
    : min(min), max(max)
  {
  }
}
