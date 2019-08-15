//
// quaternion - mathmatical quaternion class
//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#pragma once

#include "lml.h"
#include <leap/lml/point.h>
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

  template<typename T, typename V = Vector<T, 3>>
  class Quaternion : public VectorView<Quaternion<T, V>, T, 0, 1, 2, 3>
  {
    public:

      using scalar_t = T;
      using vector_t = V;

    public:
      Quaternion() = default;
      Quaternion(T w, T x, T y, T z);
      Quaternion(T w, V const &vector);

      explicit Quaternion(V const &axis, T angle);
      explicit Quaternion(V const &xaxis, V const &yaxis, V const &zaxis);

      union
      {
        struct
        {
          T w;

          union
          {
            struct
            {
              T x;
              T y;
              T z;
            };

            vector_t xyz;
          };
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
  template<typename T, typename V>
  Quaternion<T, V>::Quaternion(T w, T x, T y, T z)
    : scalar(w),
      vector({ x, y, z })
  {
  }


  //|///////////////////// Quaternion::Constructor //////////////////////////
  template<typename T, typename V>
  Quaternion<T, V>::Quaternion(T w, V const &vector)
    : Quaternion(w, get<0>(vector), get<1>(vector), get<2>(vector))
  {
  }


  //|///////////////////// Quaternion::Constructor //////////////////////////
  /// axis should be a unit vector
  template<typename T, typename V>
  Quaternion<T, V>::Quaternion(V const &axis, T angle)
    : Quaternion(std::cos(T(0.5)*angle), axis * std::sin(T(0.5)*angle))
  {
  }


  //|///////////////////// Quaternion::Constructor //////////////////////////
  /// basis axis
  template<typename T, typename V>
  Quaternion<T, V>::Quaternion(V const &xaxis, V const &yaxis, V const &zaxis)
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
  template<typename T, typename V>
  constexpr Quaternion<T, V> conjugate(Quaternion<T, V> const &q)
  {
    return { q.w, -q.x, -q.y, -q.z };
  }


  //|///////////////////// rotation /////////////////////////////////////////
  /// Quaternion between two unit vectors
  template<typename T, typename V>
  constexpr Quaternion<T, V> rotation(V const &u, V const &v)
  {
    auto costheta = dot(u, v);

    auto axis = orthogonal(u, v);

    return normalise(Quaternion<T, V>(1 + costheta, axis));
  }


  //|///////////////////// operator * ///////////////////////////////////////
  /// Quaternion Multiplication
  template<typename T, typename V>
  constexpr Quaternion<T, V> operator *(Quaternion<T, V> const &q1, Quaternion<T, V> const &q2)
  {
    return { q1.w*q2.w - dot(q1.vector, q2.vector), q1.w*q2.vector + q2.w*q1.vector + cross(q1.vector, q2.vector) };
  }


  //|///////////////////// transform ////////////////////////////////////////
  /// transform a point by a quaternion
  template<typename T, typename V, typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr, std::enable_if_t<std::is_same<coord_type_t<Point>, T>::value>* = nullptr>
  Point transform(Quaternion<T, V> const &q, Point const &pt)
  {
    auto result = (q * Quaternion<T, V>(0, get<0>(pt), get<1>(pt), get<2>(pt)) * conjugate(q));

    return { result.x, result.y, result.z };
  }

  template<typename T, typename V, typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr, std::enable_if_t<std::is_same<coord_type_t<Point>, T>::value>* = nullptr>
  Point operator *(Quaternion<T, V> const &q, Point const &pt)
  {
    return transform(q, pt);
  }


  //|///////////////////// Quaternion::slerp ////////////////////////////////
  /// Quaternion slerp
  template<typename T, typename V>
  Quaternion<T, V> slerp(Quaternion<T, V> const &lower, Quaternion<T, V> const &upper, T alpha)
  {
    auto costheta = dot(lower, upper);

    T flip = std::copysign(T(1), costheta);

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

  using Quaternion3f = Quaternion<float>;
  using Quaternion3d = Quaternion<double>;

  /**
   *  @}
  **/

} // namespace lml
} // namespace leap
