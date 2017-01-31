//
// quaternion - mathmatical quaternion class
//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#ifndef QUATERNION_HH
#define QUATERNION_HH

#include <leap/util.h>
#include <leap/lml/vector.h>


/**
 * \namespace leap::lml
 * \brief Leap Math Library containing mathmatical routines
 *
**/

/**
 * \defgroup lmlquaternion Mathmatical Quaternion Class
 * \brief Quaternion class and mathmatical routines
 *
**/

namespace leap { namespace lml
{

  //|-------------------- Quaternion ----------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlquaternion
   *
   * \brief Mathmatical Quaternion Class
   *
  **/

  template<typename T>
  class Quaternion : public VectorView<Quaternion<T>, T, 0, 1, 2, 3>
  {
    public:

      typedef T scalar_t;
      typedef Vector<T, 3> vector_t;

    public:
      Quaternion() = default;
      Quaternion(T w, T x, T y, T z);

      template<typename Vector, size_t... Indices, std::enable_if_t<sizeof...(Indices) == 3>* = nullptr>
      explicit Quaternion(T w, VectorView<Vector, T, Indices...> const &vector);

      template<typename Vector, size_t... Indices, std::enable_if_t<sizeof...(Indices) == 3>* = nullptr>
      explicit Quaternion(VectorView<Vector, T, Indices...> const &axis, T angle);

      template<typename Vector, size_t... Indices, size_t... Jndices, size_t... Kndices, std::enable_if_t<sizeof...(Indices) == 3 && sizeof...(Jndices) == 3 && sizeof...(Kndices) == 3>* = nullptr>
      explicit Quaternion(VectorView<Vector, T, Indices...> const &xaxis, VectorView<Vector, T, Jndices...> const &yaxis, VectorView<Vector, T, Kndices...> const &zaxis);

      union
      {
        struct
        {
          T w;
          T x;
          T y;
          T z;
        };

        struct
        {
          scalar_t scalar;
          vector_t vector;
        };
      };

      // Euler
      scalar_t ax() const { return std::atan2(2*(y*z + x*w), w*w - x*x - y*y + z*z); }
      scalar_t ay() const { return std::asin(clamp(2*(y*w - x*z)/(w*w + x*x + y*y + z*z), T(-1), T(1))); }
      scalar_t az() const { return std::atan2(2*(x*y + z*w), w*w + x*x - y*y - z*z); }

      // Basis
      vector_t xaxis() const { return { w*w + x*x - y*y - z*z, 2*(x*y + z*w), 2*(x*z - y*w) }; }
      vector_t yaxis() const { return { 2*(x*y - z*w), w*w - x*x + y*y - z*z, 2*(y*z + x*w) }; }
      vector_t zaxis() const { return { 2*(x*z + y*w), 2*(y*z - x*w), w*w - x*x - y*y + z*z }; }
  };


  //|///////////////////// Quaternion::Constructor //////////////////////////
  template<typename T>
  Quaternion<T>::Quaternion(T w, T x, T y, T z)
    : scalar(w),
      vector({ x, y, z })
  {
  }

  //|///////////////////// Quaternion::Constructor //////////////////////////
  template<typename T>
  template<typename Vector, size_t... Indices, std::enable_if_t<sizeof...(Indices) == 3>*>
  Quaternion<T>::Quaternion(T w, VectorView<Vector, T, Indices...> const &vector)
    : Quaternion(w, get<0>(vector), get<1>(vector), get<2>(vector))
  {
  }


  //|///////////////////// Quaternion::Constructor //////////////////////////
  /// axis should be a unit vector
  template<typename T>
  template<typename Vector, size_t... Indices, std::enable_if_t<sizeof...(Indices) == 3>*>
  Quaternion<T>::Quaternion(VectorView<Vector, T, Indices...> const &axis, T angle)
    : Quaternion(std::cos(T(0.5)*angle), axis * std::sin(T(0.5)*angle))
  {
  }


  //|///////////////////// Quaternion::Constructor //////////////////////////
  /// basis axis
  template<typename T>
  template<typename Vector, size_t... Indices, size_t... Jndices, size_t... Kndices, std::enable_if_t<sizeof...(Indices) == 3 && sizeof...(Jndices) == 3 && sizeof...(Kndices) == 3>*>
  Quaternion<T>::Quaternion(VectorView<Vector, T, Indices...> const &xaxis, VectorView<Vector, T, Jndices...> const &yaxis, VectorView<Vector, T, Kndices...> const &zaxis)
  {
    auto sx = get<0>(xaxis);
    auto sy = get<1>(yaxis);
    auto sz = get<2>(zaxis);

    if (sx + sy + sz > T(0))
    {
      auto s = std::sqrt(sx + sy + sz + T(1));
      auto t = T(0.5) / s;

      x = (get<2>(yaxis) - get<1>(zaxis)) * t;
      y = (get<0>(zaxis) - get<2>(xaxis)) * t;
      z = (get<1>(xaxis) - get<0>(yaxis)) * t;
      w = T(0.5) * s;
    }
    else if (sx > sy && sx > sz)
    {
      auto s = std::sqrt(sx - sy - sz + T(1));
      auto t = T(0.5) / s;

      x = T(0.5) * s;
      y = (get<0>(yaxis) + get<1>(xaxis)) * t;
      z = (get<2>(xaxis) + get<0>(zaxis)) * t;
      w = (get<2>(yaxis) - get<1>(zaxis)) * t;
    }
    else if (sy > sz)
    {
      auto s = std::sqrt(-sx + sy - sz + T(1));
      auto t = T(0.5) / s;

      x = (get<0>(yaxis) + get<1>(xaxis)) * t;
      y = T(0.5) * s;
      z = (get<1>(zaxis) + get<2>(yaxis)) * t;
      w = (get<0>(zaxis) - get<2>(xaxis)) * t;
    }
    else
    {
      auto s = std::sqrt(-sx - sy + sz + T(1));
      auto t = T(0.5) / s;

      x = (get<0>(zaxis) + get<2>(xaxis)) * t;
      y = (get<1>(zaxis) + get<2>(yaxis)) * t;
      z = T(0.5) * s;
      w = (get<1>(xaxis) - get<0>(yaxis)) * t;
    }
  }



  /**
   * \name General Quaternion Operations
   * \ingroup lmlquaternion
   * Generic operations for quaternions
   * @{
  **/


  //|///////////////////// conjugate ////////////////////////////////////////
  /// Conjugate a quaternion
  template<typename T>
  Quaternion<T> conjugate(Quaternion<T> const &q)
  {
    return Quaternion<T>(q.w, -q.x, -q.y, -q.z);
  }


  //|///////////////////// operator * ///////////////////////////////////////
  /// Quaternion Multiplication
  template<typename T>
  Quaternion<T> operator *(Quaternion<T> const &q1, Quaternion<T> const &q2)
  {
    return Quaternion<T>(q1.w*q2.w - dot(q1.vector, q2.vector), q1.w*q2.vector + q2.w*q1.vector + cross(q1.vector, q2.vector));
  }


  //|///////////////////// Quaternion::rotate ///////////////////////////////
  /// Quaternion Vector Rotation
  template<typename Vector, typename T, size_t... Indices, std::enable_if_t<sizeof...(Indices) == 3>* = nullptr>
  Vector operator *(Quaternion<T> const &q, VectorView<Vector, T, Indices...> const &v)
  {
    auto result = (q * Quaternion<T>(0, get<0>(v), get<1>(v), get<2>(v)) * conjugate(q));

    return { result.x, result.y, result.z };
  }


  //|///////////////////// rotation /////////////////////////////////////////
  /// quaternion between two unit vectors
  template<typename T, typename Vector, size_t... Indices, size_t... Jndices>
  Quaternion<T> rotation(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v)
  {
    auto costheta = dot(u, v);

    auto axis = orthogonal(u, v);

    return normalise(Quaternion<T>(1 + costheta, get<0>(axis), get<1>(axis), get<2>(axis)));
  }


  //|///////////////////// Quaternion::slerp ////////////////////////////////
  /// Quaternion slerp
  template<typename T>
  Quaternion<T> slerp(Quaternion<T> const &lower, Quaternion<T> const &upper, T alpha)
  {
    auto costheta = dot(lower, upper);

    T flip = std::copysign(1, costheta);

    if (costheta < -T(0.95) || costheta > T(0.95))
      return lerp(lower, flip*upper, alpha);

    auto theta = std::acos(flip*costheta);

    return (std::sin(theta*(1-alpha))*lower + std::sin(theta*alpha)*flip*upper) / std::sin(theta);
  }


  /**
   *  @}
  **/


  /**
   * \name Misc Quaternion
   * \ingroup lmlquaternion
   * Quaternion helpers
   * @{
  **/

  typedef Quaternion<float> Quaternion3f;
  typedef Quaternion<double> Quaternion3d;


  /**
   *  @}
  **/

} // namespace lml
} // namespace leap

#endif
