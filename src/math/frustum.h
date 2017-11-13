//
// Datum - frustum
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "vec.h"
#include "bound.h"
#include "plane.h"
#include "sphere.h"
#include "transform.h"

namespace lml
{

  //|---------------------- Frustum -----------------------------------------
  //|------------------------------------------------------------------------

  class Frustum
  {
    public:
      Frustum() = default;
      constexpr Frustum(Vec3 const (&corners)[8]);

      static constexpr Frustum perspective(float fov, float aspect, float znear, float zfar);
      static constexpr Frustum perspective(float left, float bottom, float right, float top, float znear, float zfar);
      static constexpr Frustum orthographic(float left, float bottom, float right, float top, float znear, float zfar);

      Vec3 centre() const;

      Plane planes[6];
      Vec3 corners[8];
  };


  ///////////////////////// Frustum::Constructor ////////////////////////////
  constexpr Frustum::Frustum(Vec3 const (&corners)[8])
    : corners{corners[0], corners[1], corners[2], corners[3], corners[4], corners[5], corners[6], corners[7]}
  {
    planes[0] = Plane(corners[2], corners[1], corners[0]); // Near
    planes[1] = Plane(corners[0], corners[4], corners[7]); // Left
    planes[2] = Plane(corners[6], corners[5], corners[1]); // Right
    planes[3] = Plane(corners[3], corners[7], corners[6]); // Top
    planes[4] = Plane(corners[1], corners[5], corners[4]); // Bottom
    planes[5] = Plane(corners[5], corners[6], corners[7]); // Far
  }


  ///////////////////////// Frustum::perspective ////////////////////////////
  constexpr Frustum Frustum::perspective(float fov, float aspect, float znear, float zfar)
  {
    Vec3 corners[8];

    float scale = std::tan(fov/2);

    corners[0] = Vec3(-znear*scale*aspect, -znear*scale, -znear);
    corners[1] = Vec3(znear*scale*aspect, -znear*scale, -znear);
    corners[2] = Vec3(znear*scale*aspect, znear*scale, -znear);
    corners[3] = Vec3(-znear*scale*aspect, znear*scale, -znear);

    corners[4] = Vec3(-zfar*scale*aspect, -zfar*scale, -zfar);
    corners[5] = Vec3(zfar*scale*aspect, -zfar*scale, -zfar);
    corners[6] = Vec3(zfar*scale*aspect, zfar*scale, -zfar);
    corners[7] = Vec3(-zfar*scale*aspect, zfar*scale, -zfar);

    return Frustum(corners);
  }


  ///////////////////////// Frustum::perspective ////////////////////////////
  constexpr Frustum Frustum::perspective(float left, float bottom, float right, float top, float znear, float zfar)
  {
    Vec3 corners[8];

    corners[0] = Vec3(left, bottom, -znear);
    corners[1] = Vec3(right, bottom, -znear);
    corners[2] = Vec3(right, top, -znear);
    corners[3] = Vec3(left, top, -znear);

    corners[4] = Vec3(left*zfar/znear, bottom*zfar/znear, -zfar);
    corners[5] = Vec3(right*zfar/znear, bottom*zfar/znear, -zfar);
    corners[6] = Vec3(right*zfar/znear, top*zfar/znear, -zfar);
    corners[7] = Vec3(left*zfar/znear, top*zfar/znear, -zfar);

    return Frustum(corners);
  }


  ///////////////////////// Frustum::orthographic ///////////////////////////
  constexpr Frustum Frustum::orthographic(float left, float bottom, float right, float top, float znear, float zfar)
  {
    Vec3 corners[8];

    corners[0] = Vec3(left, bottom, -znear);
    corners[1] = Vec3(right, bottom, -znear);
    corners[2] = Vec3(right, top, -znear);
    corners[3] = Vec3(left, top, -znear);

    corners[4] = Vec3(left, bottom, -zfar);
    corners[5] = Vec3(right, bottom, -zfar);
    corners[6] = Vec3(right, top, -zfar);
    corners[7] = Vec3(left, top, -zfar);

    return Frustum(corners);
  }


  ///////////////////////// Frustum::centre ////////////////////////////
  inline Vec3 Frustum::centre() const
  {
    Vec3 result = Vec3(0, 0, 0);

    for(size_t i = 0; i < 8; ++i)
     result += corners[i];

    return result / 8;
  }


  //////////////////////// Frustum operator == //////////////////////////////
  constexpr bool operator ==(Frustum const &lhs, Frustum const &rhs)
  {
    bool result = true;

    for(size_t i = 0; i < 8; ++i)
      result &= (lhs.corners[i] == rhs.corners[i]);

    return result;
  }


  //////////////////////// Frustum operator != //////////////////////////////
  constexpr bool operator !=(Frustum const &lhs, Frustum const &rhs)
  {
    return !(lhs == rhs);
  }


  //////////////////////// Frustum stream << /////////////////////////////////
  inline std::ostream &operator <<(std::ostream &os, Frustum const &frustum)
  {
    os << "[Frustum:" << frustum.corners[0] << "," << frustum.corners[1] << "," << frustum.corners[2] << "," << frustum.corners[3] << "," << frustum.corners[4] << "," << frustum.corners[5] << "," << frustum.corners[6] << "," << frustum.corners[7] << "]";

    return os;
  }


  //////////////////////// operator * ///////////////////////////////////////
  /// Transform frustum
  inline Frustum operator *(Transform const &transform, Frustum const &frustum)
  {
    Vec3 corners[8];

    corners[0] = transform * frustum.corners[0];
    corners[1] = transform * frustum.corners[1];
    corners[2] = transform * frustum.corners[2];
    corners[3] = transform * frustum.corners[3];

    corners[4] = transform * frustum.corners[4];
    corners[5] = transform * frustum.corners[5];
    corners[6] = transform * frustum.corners[6];
    corners[7] = transform * frustum.corners[7];

    return Frustum(corners);
  }


  //////////////////////// contains /////////////////////////////////////////
  inline bool contains(Frustum const &frustum, Vec3 const &pt)
  {
    for(int i = 0; i < 6; ++i)
    {
      if (orientation(frustum.planes[i], pt) < 0.0f)
        return false;
    }

    return true;
  }


  //////////////////////// contains /////////////////////////////////////////
  inline bool contains(Frustum const &frustum, Sphere const &sphere)
  {
    for(int i = 0; i < 6; ++i)
    {
      if (orientation(frustum.planes[i], sphere.centre) < sphere.radius)
        return false;
    }

    return true;
  }


  //////////////////////// contains /////////////////////////////////////////
  inline bool contains(Frustum const &frustum, Bound3 const &box)
  {
    auto centre = box.centre();
    auto halfdim = box.halfdim();

    for(int i = 0; i < 6; ++i)
    {
      if (orientation(frustum.planes[i], centre) < dot(abs(frustum.planes[i].normal), halfdim))
        return false;
    }

    return true;
  }


  //////////////////////// intersects ///////////////////////////////////////
  inline bool intersects(Frustum const &frustum, Sphere const &sphere)
  {
    for(int i = 0; i < 6; ++i)
    {
      if (orientation(frustum.planes[i], sphere.centre) < -sphere.radius)
        return false;
    }

    return true;
  }


  //////////////////////// intersects ///////////////////////////////////////
  inline bool intersects(Frustum const &frustum, Bound3 const &box)
  {
#if 0
    auto centre = box.centre();
    auto halfdim = box.halfdim();

    for(int i = 0; i < 6; ++i)
    {
      if (dot(frustum.planes[i].normal, centre) + frustum.planes[i].distance < -dot(abs(frustum.planes[i].normal), halfdim))
        return false;
    }
#else
    for(int i = 0; i < 6; ++i)
    {
      auto test = box.min;

      if (frustum.planes[i].normal.x >= 0) test.x = box.max.x;
      if (frustum.planes[i].normal.y >= 0) test.y = box.max.y;
      if (frustum.planes[i].normal.z >= 0) test.z = box.max.z;

      if (dot(frustum.planes[i].normal, test) + frustum.planes[i].distance < 0.0f)
        return false;
    }
#endif

    return true;
  }

}
