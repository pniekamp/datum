//
// Datum - plane
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "vec.h"
#include "transform.h"

namespace lml
{
  //|---------------------- Plane -------------------------------------------
  //|------------------------------------------------------------------------

  class Plane
  {
    public:
      Plane() = default;
      constexpr Plane(Vec3 const &normal, float distance);
      explicit constexpr Plane(Vec3 const &normal, Vec3 const &pt);
      explicit constexpr Plane(Vec3 const &a, Vec3 const &b, Vec3 const &c);

      Vec3 normal;
      float distance;
  };


  ///////////////////////// Plane::Constructor //////////////////////////////
  constexpr Plane::Plane(Vec3 const &normal, float distance)
    : normal(normal),
      distance(distance)
  {
  }


  ///////////////////////// Plane::Constructor //////////////////////////////
  constexpr Plane::Plane(Vec3 const &normal, Vec3 const &pt)
    : Plane(normal, -dot(normal, pt))
  {
  }


  ///////////////////////// Plane::Constructor //////////////////////////////
  constexpr Plane::Plane(Vec3 const &a, Vec3 const &b, Vec3 const &c)
    : Plane(normalise(cross(b - a, c - a)), a)
  {
  }


  //////////////////////// Plane operator == ////////////////////////////////
  constexpr bool operator ==(Plane const &lhs, Plane const &rhs)
  {
    return (lhs.normal == rhs.normal) && (lhs.distance == rhs.distance);
  }


  //////////////////////// Plane operator != ////////////////////////////////
  constexpr bool operator !=(Plane const &lhs, Plane const &rhs)
  {
    return !(lhs == rhs);
  }


  //////////////////////// Plane stream << //////////////////////////////////
  inline std::ostream &operator <<(std::ostream &os, Plane const &plane)
  {
    os << "[Plane:" << plane.normal << "," << plane.distance << "]";

    return os;
  }


  //////////////////////// normalise ////////////////////////////////////////
  inline Plane normalise(Plane const &plane)
  {
    float scale = 1/norm(plane.normal);

    return Plane(plane.normal * scale, plane.distance * scale);
  }


  //////////////////////// dist /////////////////////////////////////////////
  /// distance between plane and point
  inline float dist(Plane const &plane, Vec3 const &pt)
  {
    return std::abs(dot(plane.normal, pt) + plane.distance);
  }


  //////////////////////// orientation //////////////////////////////////////
  inline float orientation(Plane const &plane, Vec3 const &pt)
  /// orientation (signed distance) of plane to point, behind < 0, above > 0
  {
    float result = dot(plane.normal, pt) + plane.distance;

    return fcmp(result, decltype(result)(0)) ? 0 : result;
  }


  //////////////////////// nearest_on_plane /////////////////////////////////
  /// nearest point on plane
  inline Vec3 nearest_on_plane(Plane const &plane, Vec3 const &pt)
  {
    return pt - plane.normal * (dot(plane.normal, pt) + plane.distance);
  }


  //////////////////////// transform ////////////////////////////////////////
  /// transform plane
  inline Plane operator *(Transform const &transform, Plane const &plane)
  {
    auto result = transform.matrix() * Vec4(plane.normal.x, plane.normal.y, plane.normal.z, plane.distance);

    return normalise(Plane(result.xyz, result.w));
  }

}
