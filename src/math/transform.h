//
// Datum - transform
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "vec.h"
#include <leap/lml/quaternion.h>

namespace lml
{
  using namespace leap::lml;

  //|-------------------- Transform -----------------------------------------
  //|------------------------------------------------------------------------

  class Transform
  {
    public:
      Transform() = default;

      static Transform identity();
      static Transform rotation(Quaternion3f const &quaternion);
      static Transform rotation(Vec3 const &axis, float angle);
      static Transform translation(Vec3 const &vector);
      static Transform translation(float x, float y, float z);
      static Transform lookat(Vec3 const &eye, Vec3 const &target, Vec3 const &up);

      Quaternion3f rotation() const { return real; }

      Vec3 translation() const { return 2.0f * (dual*conjugate(real)).vector.to<Vec3>(); }

      Matrix4f matrix() const;

      Quaternion3f real;
      Quaternion3f dual;
  };


  //////////////////////// Transform::identity //////////////////////////////
  inline Transform Transform::identity()
  {
    return { Quaternion3f(1.0f, 0.0f, 0.0f, 0.0f), Quaternion3f(0.0f, 0.0f, 0.0f, 0.0f) };
  }


  //////////////////////// Transform::rotation //////////////////////////////
  inline Transform Transform::rotation(Quaternion3f const &quaternion)
  {
    return { quaternion, Quaternion3f(0.0f, 0.0f, 0.0f, 0.0f) };
  }


  //////////////////////// Transform::rotation //////////////////////////////
  inline Transform Transform::rotation(Vec3 const &axis, float angle)
  {
    return { Quaternion3f(axis, angle), Quaternion3f(0.0f, 0.0f, 0.0f, 0.0f) };
  }


  //////////////////////// Transform::translation ///////////////////////////
  inline Transform Transform::translation(Vec3 const &vector)
  {
    return { Quaternion3f(1.0f, 0.0f, 0.0f, 0.0f), Quaternion3f(0.0f, 0.5f*vector.x, 0.5f*vector.y, 0.5f*vector.z) };
  }


  //////////////////////// Transform::translation ///////////////////////////
  inline Transform Transform::translation(float x, float y, float z)
  {
    return { Quaternion3f(1.0f, 0.0f, 0.0f, 0.0f), Quaternion3f(0.0f, 0.5f*x, 0.5f*y, 0.5f*z) };
  }


  //////////////////////// Transform operator == ////////////////////////////
  inline bool operator ==(Transform const &lhs, Transform const &rhs)
  {
    return (lhs.real == rhs.real) && (lhs.dual == rhs.dual);
  }


  //////////////////////// Transform operator != ////////////////////////////
  inline bool operator !=(Transform const &lhs, Transform const &rhs)
  {
    return !(lhs == rhs);
  }


  //////////////////////// Transform stream << //////////////////////////////
  inline std::ostream &operator <<(std::ostream &os, Transform const &transform)
  {
    os << "[Transform:" << transform.real << "," << transform.dual << "]";

    return os;
  }


  //////////////////////// normalise ////////////////////////////////////////
  /// normalise transform
  inline Transform normalise(Transform const &t)
  {
    auto len = norm(t.real);
    auto real = t.real/len;
    auto dual = (t.dual*len - t.real*dot(t.real, t.dual)/len)/(len*len);

    return { real, dual };
  }


  //////////////////////// conjugate ////////////////////////////////////////
  /// conjugate transform
  inline Transform conjugate(Transform const &t)
  {
    auto real = Quaternion3f(t.real.scalar, -t.real.vector);
    auto dual = Quaternion3f(-t.dual.scalar, t.dual.vector);

    return { real, dual };
  }


  //////////////////////// inverse //////////////////////////////////////////
  /// inverse transform
  inline Transform inverse(Transform const &t)
  {
    auto real = Quaternion3f(t.real.scalar, -t.real.vector);
    auto dual = Quaternion3f(t.dual.scalar, -t.dual.vector);

    return { real, dual };
  }


  //////////////////////// operator * ///////////////////////////////////////
  /// Transform Multiplication
  inline Transform operator *(Transform const &t1, Transform const &t2)
  {
    auto real = t1.real*t2.real;
    auto dual = t1.real*t2.dual + t1.dual*t2.real;

    return { real, dual };
  }


  //////////////////////// transform ////////////////////////////////////////
  /// Transform Point
  inline Vec3 operator *(Transform const &t, Vec3 const &v)
  {
    auto result = t * Transform{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, v.x, v.y, v.z}} * conjugate(t);

    return { result.dual.x, result.dual.y, result.dual.z };
  }


  //////////////////////// lookat ///////////////////////////////////////////
  inline Transform Transform::lookat(Vec3 const &eye, Vec3 const &target, Vec3 const &up)
  {
    auto zaxis = normalise(eye - target);
    auto xaxis = normalise(orthogonal(up, zaxis));
    auto yaxis = cross(zaxis, xaxis);

    return Transform::translation(eye) * Transform::rotation(Quaternion3f(xaxis, yaxis, zaxis));
  }


  //////////////////////// matrix ///////////////////////////////////////////
  inline Matrix4f Transform::matrix() const
  {
    Matrix4f result;

    auto shift = translation();

    result(0, 0) = 1 - 2*real.y*real.y - 2*real.z*real.z;
    result(1, 0) = 2*real.x*real.y + 2*real.z*real.w;
    result(2, 0) = 2*real.x*real.z - 2*real.y*real.w;
    result(3, 0) = 0.0f;
    result(0, 1) = 2*real.x*real.y - 2*real.z*real.w;
    result(1, 1) = 1 - 2*real.x*real.x - 2*real.z*real.z;
    result(2, 1) = 2*real.y*real.z + 2*real.x*real.w;
    result(3, 1) = 0.0f;
    result(0, 2) = 2*real.x*real.z + 2*real.y*real.w;
    result(1, 2) = 2*real.y*real.z - 2*real.x*real.w;
    result(2, 2) = 1 - 2*real.x*real.x - 2*real.y*real.y;
    result(3, 2) = 0.0f;
    result(0, 3) = shift.x;
    result(1, 3) = shift.y;
    result(2, 3) = shift.z;
    result(3, 3) = 1.0f;

    return result;
  }

}

