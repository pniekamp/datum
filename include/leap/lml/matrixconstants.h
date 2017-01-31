//
// matrixconstants - mathmatical matrix constants
//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#ifndef LMLMATRIXCONSTANTS_HH
#define LMLMATRIXCONSTANTS_HH

#include <leap/lml/matrix.h>
#include <leap/lml/quaternion.h>
#include <cmath>


namespace leap { namespace lml
{
  /**
   * \defgroup lmlmatrixconstants Matrix Constants
   * \ingroup lmlmatrix
   * Predefined Matrix Constants
   *
   * \code
   * #include <leap/lml/matrixconstants.h>
   * \endcode
   *
  **/


  //|-------------------- ZeroMatrix ----------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrixconstants
   *
   * \brief Zero Matrix Constant
   *
   * \see lmlmatrix
  **/

  template<typename T, size_t M, size_t N>
  Matrix<T, M, N> ZeroMatrix()
  {
    return Matrix<T, M, N>{};
  }


  //|-------------------- IdentityMatrix ------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrixconstants
   *
   * \brief Identity Matrix Constant
   *
   * \see lmlmatrix
  **/

  template<typename T, size_t N>
  Matrix<T, N, N> IdentityMatrix()
  {
    Matrix<T, N, N> result = {};

    for(size_t i = 0; i < N; ++i)
      result(i, i) = 1;

    return result;
  }


  //|-------------------- BasisMatrix ---------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrixconstants
   *
   * \brief Basis Matrix Constant
   *
   * \see lmlmatrix
  **/

  template<typename V, typename T, size_t... Indices, size_t... Jndices>
  Matrix<T, 2, 2> BasisMatrix(VectorView<V, T, Indices...> const &i, VectorView<V, T, Jndices...> const &j)
  {
    Matrix<T, 2, 2> result;

    result(0, 0) = get<0>(i);
    result(1, 0) = get<1>(i);
    result(0, 1) = get<0>(j);
    result(1, 1) = get<1>(j);

    return result;
  }

  template<typename V, typename T, size_t... Indices, size_t... Jndices, size_t... Kndices>
  Matrix<T, 3, 3> BasisMatrix(VectorView<V, T, Indices...> const &i, VectorView<V, T, Jndices...> const &j, VectorView<V, T, Kndices...> const &k)
  {
    Matrix<T, 3, 3> result;

    result(0, 0) = get<0>(i);
    result(1, 0) = get<1>(i);
    result(2, 0) = get<2>(i);
    result(0, 1) = get<0>(j);
    result(1, 1) = get<1>(j);
    result(2, 1) = get<2>(j);
    result(0, 2) = get<0>(k);
    result(1, 2) = get<1>(k);
    result(2, 2) = get<2>(k);

    return result;
  }


  //|-------------------- ScaleMatrix -------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrixconstants
   *
   * \brief Scaling Matrix Constant
   *
   * \see lmlmatrix
  **/

  template<typename T>
  Matrix<T, 2, 2> ScaleMatrix(T sx, T sy)
  {
    Matrix<T, 2, 2> result = {};

    result(0, 0) = sx;
    result(1, 1) = sy;

    return result;
  }

  template<typename T>
  Matrix<T, 3, 3> ScaleMatrix(T sx, T sy, T sz)
  {
    Matrix<T, 3, 3> result = {};

    result(0, 0) = sx;
    result(1, 1) = sy;
    result(2, 2) = sz;

    return result;
  }

  template<typename T>
  Matrix<T, 4, 4> ScaleMatrix(T sx, T sy, T sz, T sw)
  {
    Matrix<T, 4, 4> result = {};

    result(0, 0) = sx;
    result(1, 1) = sy;
    result(2, 2) = sz;
    result(3, 3) = sw;

    return result;
  }

  template<typename T, size_t N>
  Matrix<T, N, N> ScaleMatrix(Vector<T, N> const &scale)
  {
    Matrix<T, N, N> result = {};

    for(size_t i = 0; i < N; ++i)
      result(i, i) = scale(i);

    return result;
  }

  template<typename V, typename T, size_t... Indices>
  Matrix<T, sizeof...(Indices), sizeof...(Indices)> ScaleMatrix(VectorView<V, T, Indices...> const &scale)
  {
    return ScaleMatrix(Vector<T, sizeof...(Indices)>::from(scale));
  }


  //|-------------------- RotationMatrix ------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrixconstants
   *
   * \brief Rotation Matrix Constant
   *
   * \see lmlmatrix
  **/

  template<typename T>
  Matrix<T, 2, 2> RotationMatrix(T angle)
  {
    Matrix<T, 2, 2> result = {};

    using std::cos;
    using std::sin;

    result(0, 0) = cos(angle);
    result(1, 0) = sin(angle);
    result(0, 1) = -sin(angle);
    result(1, 1) = cos(angle);

    return result;
  }

  template<typename V, typename T, size_t... Indices, std::enable_if_t<sizeof...(Indices) == 3>* = nullptr>
  Matrix<T, sizeof...(Indices), sizeof...(Indices)> RotationMatrix(VectorView<V, T, Indices...> const &axis, T angle)
  {
    Matrix<T, 3, 3> result = {};

    using std::cos;
    using std::sin;

    T x = get<0>(axis);
    T y = get<1>(axis);
    T z = get<2>(axis);

    result(0, 0) = 1 + (1-cos(angle))*(x*x-1);
    result(1, 0) = z*sin(angle)+(1-cos(angle))*x*y;
    result(2, 0) = -y*sin(angle)+(1-cos(angle))*x*z;
    result(0, 1) = -z*sin(angle)+(1-cos(angle))*x*y;
    result(1, 1) = 1 + (1-cos(angle))*(y*y-1);
    result(2, 1) = x*sin(angle)+(1-cos(angle))*y*z;
    result(0, 2) = y*sin(angle)+(1-cos(angle))*x*z;
    result(1, 2) = -x*sin(angle)+(1-cos(angle))*y*z;
    result(2, 2) = 1 + (1-cos(angle))*(z*z-1);

    return result;
  }

  template<typename T>
  Matrix<T, 3, 3> RotationMatrix(Quaternion<T> const &q)
  {
    Matrix<T, 3, 3> result = {};

    result(0, 0) = 1 - 2*q.y*q.y - 2*q.z*q.z;
    result(1, 0) = 2*q.x*q.y + 2*q.z*q.w;
    result(2, 0) = 2*q.x*q.z - 2*q.y*q.w;
    result(0, 1) = 2*q.x*q.y - 2*q.z*q.w;
    result(1, 1) = 1 - 2*q.x*q.x - 2*q.z*q.z;
    result(2, 1) = 2*q.y*q.z + 2*q.x*q.w;
    result(0, 2) = 2*q.x*q.z + 2*q.y*q.w;
    result(1, 2) = 2*q.y*q.z - 2*q.x*q.w;
    result(2, 2) = 1 - 2*q.x*q.x - 2*q.y*q.y;

    return result;
  }


  //|-------------------- AffineMatrix --------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrixconstants
   *
   * \brief Affine Matrix Constant
   *
   * \see lmlmatrix
  **/

  template<typename T, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, N+1, N+1, B> AffineMatrix(Matrix<T, N, N, B> const &linear, Vector<T, N> const &translation)
  {
    Matrix<T, N+1, N+1, B> result = {};

    for(size_t j = 0; j < N; ++j)
      for(size_t i = 0; i < N; ++i)
        result(i, j) = linear(i, j);

    for(size_t i = 0; i < N; ++i)
      result(i, N) = translation(i);

    for(size_t j = 0; j < N; ++j)
      result(N, j) = 0;

    result(N, N) = 1;

    return result;
  }

  template<typename T, size_t N, template<typename, size_t, size_t> class B, typename V, size_t... Indices, std::enable_if_t<sizeof...(Indices) == N>* = nullptr>
  Matrix<T, N+1, N+1, B> AffineMatrix(Matrix<T, N, N, B> const &linear, VectorView<V, T, Indices...> const &translation)
  {
    return AffineMatrix(linear, Vector<T, sizeof...(Indices)>::from(translation));
  }


  //|-------------------- LookAtMatrix --------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrixconstants
   *
   * \brief Look At Matrix
   *
   * \see lmlmatrix
  **/

  template<typename V, typename T, size_t... Indices, size_t... Jndices, size_t... Kndices, std::enable_if_t<sizeof...(Indices) == 3 && sizeof...(Jndices) == 3 && sizeof...(Kndices) == 3>* = nullptr>
  Matrix<T, 4, 4> LookAtMatrix(VectorView<V, T, Indices...> const &eye, VectorView<V, T, Jndices...> const &target, VectorView<V, T, Kndices...> const &up)
  {
    Matrix<T, 4, 4> result = {};

    auto zaxis = normalise(eye - target);
    auto xaxis = normalise(cross(up, zaxis));
    auto yaxis = cross(zaxis, xaxis);

    result(0, 0) = get<0>(xaxis);
    result(1, 0) = get<1>(xaxis);
    result(2, 0) = get<2>(xaxis);
    result(3, 0) = 0;
    result(0, 1) = get<0>(yaxis);
    result(1, 1) = get<1>(yaxis);
    result(2, 1) = get<2>(yaxis);
    result(3, 1) = 0;
    result(0, 2) = get<0>(zaxis);
    result(1, 2) = get<1>(zaxis);
    result(2, 2) = get<2>(zaxis);
    result(3, 2) = 0;
    result(0, 3) = get<0>(eye);
    result(1, 3) = get<1>(eye);
    result(2, 3) = get<2>(eye);
    result(3, 3) = 1;

    return result;
  }


  //|-------------------- OrthographicProjection ----------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrixconstants
   *
   * \brief Orthographic Projection Matrix
   *
   * \see lmlmatrix
  **/

  template<typename T>
  Matrix<T, 4, 4> OrthographicProjection(T left, T bottom, T right, T top, T znear, T zfar)
  {
    Matrix<T, 4, 4> result = {};

    result(0, 0) = 2 / (right - left);
    result(1, 1) = 2 / (top - bottom);
    result(2, 2) = -1 / (zfar - znear);
    result(0, 3) = -(right + left) / (right - left);
    result(1, 3) = -(top + bottom) / (top - bottom);
    result(2, 3) = -znear / (zfar - znear);
    result(3, 3) = 1;

    return result;
  }


  //|-------------------- PerspectiveProjection -----------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrixconstants
   *
   * \brief Perspective Projection Matrix
   *
   * \see lmlmatrix
  **/

  template<typename T>
  Matrix<T, 4, 4> PerspectiveProjection(T fov, T aspect, T znear, T zfar)
  {
    Matrix<T, 4, 4> result = {};

    result(0, 0) = 1 / (aspect * tan(fov/2));
    result(1, 1) = 1 / tan(fov/2);
    result(2, 2) = -zfar / (zfar - znear);
    result(3, 2) = -1;
    result(2, 3) = -zfar * znear / (zfar - znear);

    return result;
  }

  template<typename T>
  Matrix<T, 4, 4> PerspectiveProjection(T left, T bottom, T right, T top, T znear, T zfar)
  {
    Matrix<T, 4, 4> result = {};

    result(0, 0) = 2 * znear / (right - left);
    result(1, 1) = 2 * znear / (top - bottom);
    result(0, 2) = (right + left) / (right - left);
    result(1, 2) = (top + bottom) / (top - bottom);
    result(2, 2) = -zfar / (zfar - znear);
    result(3, 2) = -1;
    result(2, 3) = -zfar * znear / (zfar - znear);

    return result;
  }

} // namespace lml
} // namespace leap

#endif
