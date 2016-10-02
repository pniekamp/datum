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
    return Matrix<T, M, N>(0);
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
    Matrix<T, N, N> result(0);

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

  template<typename T>
  Matrix<T, 2, 2> BasisMatrix(Vector<T, 2> const &i, Vector<T, 2> const &j)
  {
    Matrix<T, 2, 2> result = {};

    result(0, 0) = i(0);
    result(1, 0) = i(1);
    result(0, 1) = j(0);
    result(1, 1) = j(1);

    return result;
  }

  template<typename T>
  Matrix<T, 3, 3> BasisMatrix(Vector<T, 3> const &i, Vector<T, 3> const &j, Vector<T, 3> const &k)
  {
    Matrix<T, 3, 3> result = {};

    result(0, 0) = i(0);
    result(1, 0) = i(1);
    result(2, 0) = i(2);
    result(0, 1) = j(0);
    result(1, 1) = j(1);
    result(2, 1) = j(2);
    result(0, 2) = k(0);
    result(1, 2) = k(1);
    result(2, 2) = k(2);

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
    Matrix<T, 2, 2> result(0);

    result(0, 0) = sx;
    result(1, 1) = sy;

    return result;
  }

  template<typename T>
  Matrix<T, 3, 3> ScaleMatrix(T sx, T sy, T sz)
  {
    Matrix<T, 3, 3> result(0);

    result(0, 0) = sx;
    result(1, 1) = sy;
    result(2, 2) = sz;

    return result;
  }

  template<typename T, size_t N>
  Matrix<T, N, N> ScaleMatrix(Vector<T, N> const &scaling)
  {
    Matrix<T, N, N> result(0);

    for(size_t i = 0; i < N; ++i)
      result(i, i) = scaling(i);

    return result;
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

  template<typename T>
  Matrix<T, 3, 3> RotationMatrix(Vector<T, 3> const &axis, T angle)
  {
    Matrix<T, 3, 3> result = {};

    using std::cos;
    using std::sin;

    T x = axis(0);
    T y = axis(1);
    T z = axis(2);

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


  //|-------------------- LookAtMatrix --------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrixconstants
   *
   * \brief Look At Matrix
   *
   * \see lmlmatrix
  **/

  template<typename T>
  Matrix<T, 4, 4> LookAtMatrix(Vector<T, 3> const &eye, Vector<T, 3> const &target, Vector<T, 3> const &up)
  {
    Matrix<T, 4, 4> result = {};

    auto zaxis = normalise(eye - target);
    auto xaxis = normalise(cross(up, zaxis));
    auto yaxis = cross(zaxis, xaxis);

    result(0, 0) = xaxis(0);
    result(1, 0) = xaxis(1);
    result(2, 0) = xaxis(2);
    result(3, 0) = 0;
    result(0, 1) = yaxis(0);
    result(1, 1) = yaxis(1);
    result(2, 1) = yaxis(2);
    result(3, 1) = 0;
    result(0, 2) = zaxis(0);
    result(1, 2) = zaxis(1);
    result(2, 2) = zaxis(2);
    result(3, 2) = 0;
    result(0, 3) = eye(0);
    result(1, 3) = eye(1);
    result(2, 3) = eye(2);
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
    Matrix<T, 4, 4> result(0);

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
    Matrix<T, 4, 4> result(0);

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
    Matrix<T, 4, 4> result(0);

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
