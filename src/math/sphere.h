//
// Datum - sphere
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "vec.h"
#include "transform.h"

namespace lml
{
  //|---------------------- Sphere ------------------------------------------
  //|------------------------------------------------------------------------

  class Sphere
  {
    public:
      Sphere() = default;
      constexpr Sphere(Vec3 const &centre, float radius);
      explicit constexpr Sphere(Vec3 const &centre, Vec3 const &pt);

      Vec3 centre;
      float radius;
  };


  ///////////////////////// Sphere::Constructor /////////////////////////////
  constexpr Sphere::Sphere(Vec3 const &centre, float radius)
    : centre(centre),
      radius(radius)
  {
  }


  ///////////////////////// Sphere::Constructor /////////////////////////////
  constexpr Sphere::Sphere(Vec3 const &centre, Vec3 const &pt)
    : Sphere(centre, dist(centre, pt))
  {
  }


  //////////////////////// Sphere operator == ///////////////////////////////
  constexpr bool operator ==(Sphere const &lhs, Sphere const &rhs)
  {
    return (lhs.centre == rhs.centre) && (lhs.radius == rhs.radius);
  }


  //////////////////////// Sphere operator != ///////////////////////////////
  constexpr bool operator !=(Sphere const &lhs, Sphere const &rhs)
  {
    return !(lhs == rhs);
  }


  //////////////////////// Sphere stream << /////////////////////////////////
  inline std::ostream &operator <<(std::ostream &os, Sphere const &sphere)
  {
    os << "[Sphere:" << sphere.centre << "," << sphere.radius << "]";

    return os;
  }


  //////////////////////// dist ////////////////////////////////////////////
  /// distance between sphere and point
  constexpr float dist(Sphere const &sphere, Vec3 const &pt)
  {
    return std::max(0.0f, dist(sphere.centre, pt) - sphere.radius);
  }


  //////////////////////// nearest_in_sphere ////////////////////////////////
  /// nearest point in sphere
  inline Vec3 nearest_in_sphere(Sphere const &sphere, Vec3 const &pt)
  {
    auto distance = dist(pt, sphere.centre);

    if (distance < sphere.radius)
      return pt;

    return sphere.centre + sphere.radius/distance * (pt - sphere.centre);
  }


  //////////////////////// transform ////////////////////////////////////////
  /// transform sphere
  inline Sphere operator *(Transform const &transform, Sphere const &sphere)
  {
    return Sphere(transform * sphere.centre, sphere.radius);
  }

}
