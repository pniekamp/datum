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

  using Quaternion3 = lml::Quaternion<float, Vec3>;

  using leap::lml::lerp;
  using leap::lml::slerp;

  //|-------------------- Transform -----------------------------------------
  //|------------------------------------------------------------------------

  class Transform
  {
    public:
      Transform() = default;

      static Transform identity();
      static Transform rotation(Quaternion3 const &quaternion);
      static Transform rotation(Vec3 const &axis, float angle);
      static Transform translation(Vec3 const &vector);
      static Transform translation(float x, float y, float z);
      static Transform lookat(Vec3 const &position, Quaternion3 const &orientation);
      static Transform lookat(Vec3 const &position, Vec3 const &target, Vec3 const &up);

      Vec3 translation() const { return 2.0f * (dual*conjugate(real)).xyz; }

      Quaternion3 rotation() const { return real; }

      Matrix4f matrix() const;

      Quaternion3 real;
      Quaternion3 dual;
  };


  //////////////////////// Transform::identity //////////////////////////////
  inline Transform Transform::identity()
  {
    return { Quaternion3(1.0f, 0.0f, 0.0f, 0.0f), Quaternion3(0.0f, 0.0f, 0.0f, 0.0f) };
  }


  //////////////////////// Transform::rotation //////////////////////////////
  inline Transform Transform::rotation(Quaternion3 const &quaternion)
  {
    return { quaternion, Quaternion3(0.0f, 0.0f, 0.0f, 0.0f) };
  }


  //////////////////////// Transform::rotation //////////////////////////////
  inline Transform Transform::rotation(Vec3 const &axis, float angle)
  {
    return { Quaternion3(axis, angle), Quaternion3(0.0f, 0.0f, 0.0f, 0.0f) };
  }


  //////////////////////// Transform::translation ///////////////////////////
  inline Transform Transform::translation(Vec3 const &vector)
  {
    return { Quaternion3(1.0f, 0.0f, 0.0f, 0.0f), Quaternion3(0.0f, 0.5f*vector.x, 0.5f*vector.y, 0.5f*vector.z) };
  }


  //////////////////////// Transform::translation ///////////////////////////
  inline Transform Transform::translation(float x, float y, float z)
  {
    return { Quaternion3(1.0f, 0.0f, 0.0f, 0.0f), Quaternion3(0.0f, 0.5f*x, 0.5f*y, 0.5f*z) };
  }


  //////////////////////// Transform::lookat ////////////////////////////
  inline Transform Transform::lookat(Vec3 const &position, Quaternion3 const &orientation)
  {
    return { orientation, Quaternion3(0.0f, 0.5f*position) * orientation };
  }


  //////////////////////// Transform::lookat ////////////////////////////////
  inline Transform Transform::lookat(Vec3 const &position, Vec3 const &target, Vec3 const &up)
  {
    auto zaxis = normalise(position - target);
    auto xaxis = normalise(orthogonal(up, zaxis));
    auto yaxis = cross(zaxis, xaxis);

    return Transform::lookat(position, Quaternion3(xaxis, yaxis, zaxis));
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
    auto real = Quaternion3(t.real.scalar, -t.real.vector);
    auto dual = Quaternion3(-t.dual.scalar, t.dual.vector);

    return { real, dual };
  }


  //////////////////////// inverse //////////////////////////////////////////
  /// inverse transform
  inline Transform inverse(Transform const &t)
  {
    auto real = Quaternion3(t.real.scalar, -t.real.vector);
    auto dual = Quaternion3(t.dual.scalar, -t.dual.vector);

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


  //////////////////////// lerp /////////////////////////////////////////////
  inline Transform lerp(Transform const &t1, Transform const &t2, float alpha)
  {
    Transform result;

    auto flip = std::copysign(1.0f, dot(t1.real, t2.real));

    result.real = lerp(t1.real, flip*t2.real, alpha);
    result.dual = lerp(t1.dual, flip*t2.dual, alpha);

    return normalise(result);
  }


  //////////////////////// slerp ////////////////////////////////////////////
  inline Transform slerp(Transform const &t1, Transform const &t2, float alpha)
  {
    auto rotation = slerp(t1.rotation(), t2.rotation(), alpha);
    auto translation = lerp(t1.translation(), t2.translation(), alpha);

    return Transform::translation(translation) * Transform::rotation(rotation);
  }


  //////////////////////// blend ////////////////////////////////////////////
  inline Transform blend(Transform const &t1, Transform const &t2, float weight)
  {
    auto flip = std::copysign(1.0f, dot(t1.real, t2.real));

    return { t1.real + weight * flip * t2.real, t1.dual + weight * flip * t2.dual };
  }
}
