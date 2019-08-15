//
// matrix - mathmatical matrix class
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
#include <leap/lml/simplematrix.h>

/**
 * \namespace leap::lml
 * \brief Leap Math Library containing mathmatical routines
 *
**/

/**
 * \defgroup lmlmatrix Mathmatical Matrix Class
 * \brief Matrix class and mathmatical routines
 *
**/

namespace leap { namespace lml
{
  //|-------------------- Matrix --------------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlmatrix
   *
   * \brief Mathmatical Matrix Class
   *
   * This Matrix class wraps the use of a mathmatical matrixof MxN-dimension.
   *
   * A Matrix(MxN) has M rows and N columns. (i,j) is the i-th row and the j-th column.
   *
   * Example Usage :
   * \code
   *
   *   #include <leap/lml/io.h>
   *   #include <leap/lml/matrix.h>
   *
   *   using namespace leap::lml;
   *
   *   void Test()
   *   {
   *     // Matrix using SimpleMatrix, based on float data type
   *     Matrix<float, 2, 2> A;
   *     A(0, 0) = 1.1;
   *     A(0, 1) = 5.4;
   *     A(1, 0) = -3.2;
   *     A(1, 1) = 2.7;
   *
   *     cout << A << endl;
   *   }
   * \endcode
   *
   * \b Backends
   * The Matrix class is derived from a backend type (defined at instantiation, defaults to SimpleMatrix)
   * that is designed to act as a storage mechanism. The default SimpleMatrix uses a std::vector array
   * to store the elements. A backend that provides only storage will still provide a fully functional
   * matrix implementation as all math functions work generically on element access. However, a backend
   * may choose to provide specialisations of the math functions so as to take advantage of the store.
   *
   * \see lmlmatrix
   * \see lmlvector
  **/

  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class Backend = SimpleMatrix>
  class Matrix : public Backend<T, M, N>
  {
    public:

      using value_type = T;
      using base_type = Backend<T, M, N>;
      using data_type = typename Backend<T, M, N>::data_type;

    public:
      Matrix() = default;
      constexpr Matrix(std::initializer_list<T> m);
      constexpr Matrix(std::initializer_list<std::initializer_list<T>> m);
      explicit constexpr Matrix(T k);
      explicit constexpr Matrix(std::array<std::array<T, N>, M> const &m);

      template<typename Q, template<typename, size_t, size_t> class C>
      Matrix(Matrix<Q, M, N, C> const &other);

      template<typename Q, template<typename, size_t, size_t> class C>
      Matrix &operator =(Matrix<Q, M, N, C> const &other);

      // Storage Access
      static constexpr size_t rows() { return M; }
      static constexpr size_t columns() { return N; }

      constexpr data_type const &data() const { return base_type::data(); }
      data_type &data() { return base_type::data(); }

      // Element Access
      constexpr T const &operator()(size_t i, size_t j) const { return base_type::operator()(i, j); }
      T &operator()(size_t i, size_t j) { return base_type::operator()(i, j); }
  };


  //|///////////////////// Matrix::Constructor //////////////////////////////
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  constexpr Matrix<T, M, N, B>::Matrix(std::initializer_list<T> m)
    : B<T, M, N>()
  {
    if (m.size() != M*N)
      throw 0;

    auto v = m.begin();

    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        (*this)(i, j) = *v++;
  }


  //|///////////////////// Matrix::Constructor //////////////////////////////
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  constexpr Matrix<T, M, N, B>::Matrix(std::initializer_list<std::initializer_list<T>> m)
    : B<T, M, N>()
  {
    if (m.size() != M)
      throw 0;

    for(size_t i = 0; i < M; ++i)
    {
      if (std::next(m.begin(), i)->size() != N)
        throw 0;

      auto v = std::next(m.begin(), i)->begin();

      for(size_t j = 0; j < N; ++j)
        (*this)(i, j) = *v++;
    }
  }


  //|///////////////////// Matrix::Constructor //////////////////////////////
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  constexpr Matrix<T, M, N, B>::Matrix(T k)
    : B<T, M, N>()
  {
    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        (*this)(i, j) = k;
  }


  //|///////////////////// Matrix::Constructor //////////////////////////////
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  constexpr Matrix<T, M, N, B>::Matrix(std::array<std::array<T, N>, M> const &m)
    : B<T, M, N>()
  {
    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        (*this)(i, j) = m[i][j];
  }


  //|///////////////////// Matrix::Constructor //////////////////////////////
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  template<typename Q, template<typename, size_t, size_t> class C>
  Matrix<T, M, N, B>::Matrix(Matrix<Q, M, N, C> const &other)
    : B<T, M, N>()
  {
    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        (*this)(i, j) = other(i, j);
  }


  //|///////////////////// Matrix::operator = ///////////////////////////////
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  template<typename Q, template<typename, size_t, size_t> class C>
  Matrix<T, M, N, B> &Matrix<T, M, N, B>::operator =(Matrix<Q, M, N, C> const &other)
  {
    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        (*this)(i, j) = other(i, j);

    return *this;
  }


  //|///////////////////// Matrix operator == ///////////////////////////////
  template<size_t M, size_t N, typename Q, typename R, template<typename, size_t, size_t> class QB, template<typename, size_t, size_t> class RB>
  bool operator ==(Matrix<Q, M, N, QB> const &lhs, Matrix<R, M, N, RB> const &rhs)
  {
    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        if (lhs(i, j) != rhs(i, j))
          return false;

    return true;
  }


  //|///////////////////// Matrix operator != ///////////////////////////////
  template<size_t M, size_t N, typename Q, typename R, template<typename, size_t, size_t> class QB, template<typename, size_t, size_t> class RB>
  bool operator !=(Matrix<Q, M, N, QB> const &lhs, Matrix<R, M, N, RB> const &rhs)
  {
    return !(lhs == rhs);
  }


  //|///////////////////// Matrix get ///////////////////////////////////////
  template<size_t i, size_t j, typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  constexpr auto const &get(Matrix<T, M, N, B> const &m) noexcept
  {
    return m(i, j);
  }


  /**
   * \name General Matrix Operations
   * \ingroup lmlmatrix
   * Generic operations for matrix of any backend
   * @{
  **/

  //|///////////////////// norm /////////////////////////////////////////////
  /// norm of the matrix
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  T norm(Matrix<T, M, N, B> const &m)
  {
    T result = 0;

    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        result += m(i, j) * m(i, j);

    return std::sqrt(result);
  }


  //|///////////////////// scale ////////////////////////////////////////////
  /// scale a matrix
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B, typename S>
  Matrix<T, M, N, B> scale(Matrix<T, M, N, B> const &m, S const &s)
  {
    Matrix<T, M, N, B> result;

    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        result(i, j) = m(i, j) * s;

    return result;
  }


  //|///////////////////// transpose ////////////////////////////////////////
  /// transpose a matrix
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, N, M, B> transpose(Matrix<T, M, N, B> const &m)
  {
    Matrix<T, N, M, B> result;

    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        result(j, i) = m(i, j);

    return result;
  }


  //|///////////////////// trace ////////////////////////////////////////////
  /// trace of matrix
  template<typename T, size_t N, template<typename, size_t, size_t> class B>
  T trace(Matrix<T, N, N, B> const &m)
  {
    T result = T(0);

    for(size_t k = 0; k < N; ++k)
      result += m(k, k);

    return result;
  }


  //|///////////////////// determinant //////////////////////////////////////
  /// determinant of matrix
  template<typename T, template<typename, size_t, size_t> class B>
  T determinant(Matrix<T, 1, 1, B> const &m)
  {
    return m(0 ,0);
  }

  template<typename T, template<typename, size_t, size_t> class B>
  T determinant(Matrix<T, 2, 2, B> const &m)
  {
    return m(0 ,0)*m(1, 1) - m(0, 1)*m(1, 0);
  }

  template<typename T, size_t N, template<typename, size_t, size_t> class B>
  T determinant(Matrix<T, N, N, B> const &m)
  {
    T result = T(0);

    for(size_t k = 0; k < N; ++k)
    {
      Matrix<T, N-1, N-1, B> sub;

      for(size_t ii = 0; ii < N-1; ++ii)
        for(size_t jj = 0; jj < N-1; ++jj)
          sub(ii, jj) = m(ii + 1, jj + (jj >= k));

      result += ((k & 1) ? -1 : 1) * m(0, k)*determinant(sub);
    }

    return result;
  }


  //|///////////////////// inverse //////////////////////////////////////////
  /// inverse of matrix
  template<typename T, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, N, N, B> inverse(Matrix<T, N, N, B> const &m)
  {
    Matrix<T, N, N, B> result;

    auto scale = T(1)/determinant(m);

    for(size_t j = 0; j < N; ++j)
    {
      for(size_t i = 0; i < N; ++i)
      {
        Matrix<T, N-1, N-1, B> sub;

        for(size_t ii = 0; ii < N-1; ++ii)
          for(size_t jj = 0; jj < N-1; ++jj)
            sub(ii, jj) = m(ii + (ii >= j), jj + (jj >= i));

        result(i, j) = (((i+j) & 1) ? -1 : 1) * determinant(sub) * scale;
      }
    }

    return result;
  }


  //|///////////////////// abs //////////////////////////////////////////////
  /// elementwise abs
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, M, N, B> abs(Matrix<T, M, N, B> const &m)
  {
    Matrix<T, M, N, B> result;

    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        result(i, j) = std::abs(m(i, j));

    return result;
  }


  //|///////////////////// hada /////////////////////////////////////////////
  /// hadamard product
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, M, N, B> hada(Matrix<T, M, N, B> const &m1, Matrix<T, M, N, B> const &m2)
  {
    Matrix<T, M, N, B> result;

    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        result(i, j) = m1(i, j) * m2(i, j);

    return result;
  }

  //|///////////////////// operator + ///////////////////////////////////////
  /// Matrix Addition
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, M, N, B> operator +(Matrix<T, M, N, B> const &m1, Matrix<T, M, N, B> const &m2)
  {
    Matrix<T, M, N, B> result;

    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        result(i, j) = m1(i, j) + m2(i, j);

    return result;
  }


  //|///////////////////// operator - ///////////////////////////////////////
  /// Matrix Subtraction
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, M, N, B> operator -(Matrix<T, M, N, B> const &m1, Matrix<T, M, N, B> const &m2)
  {
    Matrix<T, M, N, B> result;

    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        result(i, j) = m1(i, j) - m2(i, j);

    return result;
  }


  //|///////////////////// operator * ///////////////////////////////////////
  /// Matrix Multiplication
  template<size_t M, size_t N, size_t O, typename T, template<typename, size_t, size_t> class B>
  Matrix<T, M, N, B> operator *(Matrix<T, M, O, B> const &m1, Matrix<T, O, N, B> const &m2)
  {
    Matrix<T, M, N, B> result;

    for(size_t i = 0; i < M; ++i)
    {
      for(size_t j = 0; j < N; ++j)
      {
        result(i, j) = 0;

        for(size_t p = 0; p < O; ++p)
          result(i, j) += m1(i, p) * m2(p, j);
      }
    }

    return result;
  }


  //|///////////////////// operator * ///////////////////////////////////////
  /// Matrix multiplication by scalar
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B, typename S, std::enable_if_t<std::is_arithmetic<S>::value>* = nullptr>
  Matrix<T, M, N, B> operator *(S const &s, Matrix<T, M, N, B> const &m)
  {
    return scale(m, s);
  }


  //|///////////////////// operator * ///////////////////////////////////////
  /// Matrix multiplication by scalar
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B, typename S, std::enable_if_t<std::is_arithmetic<S>::value>* = nullptr>
  Matrix<T, M, N, B> operator *(Matrix<T, M, N, B> const &m, S const &s)
  {
    return scale(m, s);
  }


  //|///////////////////// operator / ///////////////////////////////////////
  /// Matrix division by scalar
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B, typename S, std::enable_if_t<std::is_arithmetic<S>::value>* = nullptr>
  Matrix<T, M, N, B> operator /(Matrix<T, M, N, B> const &m, S const &s)
  {
    return scale(m, T(1) / s);
  }


  //|///////////////////// operator * ///////////////////////////////////////
  /// Matrix Multiplication by Vector
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B, std::enable_if_t<!(N == 2 && M == 2) && !(N == 3 && M == 3) && !(N == 4 && M == 4)>* = nullptr>
  Vector<T, M> operator *(Matrix<T, M, N, B> const &m, Vector<T, N> const &v)
  {
    Vector<T, M> result;

    for(size_t i = 0; i < M; ++i)
    {
      result(i) = 0;

      for(size_t j = 0; j < N; ++j)
        result(i) += m(i, j) * v(j);
    }

    return result;
  }


  //|///////////////////// transform ////////////////////////////////////////
  /// transform a point by a transform matrix

  template<typename T, template<typename, size_t, size_t> class B, typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 2>* = nullptr, std::enable_if_t<std::is_same<coord_type_t<Point>, T>::value>* = nullptr>
  Point transform(Matrix<T, 2, 2, B> const &m, Point const &pt)
  {
    auto x = m(0,0)*get<0>(pt) + m(0,1)*get<1>(pt);
    auto y = m(1,0)*get<0>(pt) + m(1,1)*get<1>(pt);

    return { x, y };
  }

  template<typename T, template<typename, size_t, size_t> class B, typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr, std::enable_if_t<std::is_same<coord_type_t<Point>, T>::value>* = nullptr>
  Point transform(Matrix<T, 3, 3, B> const &m, Point const &pt)
  {
    auto x = m(0,0)*get<0>(pt) + m(0,1)*get<1>(pt) + m(0,2)*get<2>(pt);
    auto y = m(1,0)*get<0>(pt) + m(1,1)*get<1>(pt) + m(1,2)*get<2>(pt);
    auto z = m(2,0)*get<0>(pt) + m(2,1)*get<1>(pt) + m(2,2)*get<2>(pt);

    return { x, y, z };
  }

  template<typename T, template<typename, size_t, size_t> class B, typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 4>* = nullptr, std::enable_if_t<std::is_same<coord_type_t<Point>, T>::value>* = nullptr>
  Point transform(Matrix<T, 4, 4, B> const &m, Point const &pt)
  {
    auto x = m(0,0)*get<0>(pt) + m(0,1)*get<1>(pt) + m(0,2)*get<2>(pt) + m(0,3)*get<3>(pt);
    auto y = m(1,0)*get<0>(pt) + m(1,1)*get<1>(pt) + m(1,2)*get<2>(pt) + m(1,3)*get<3>(pt);
    auto z = m(2,0)*get<0>(pt) + m(2,1)*get<1>(pt) + m(2,2)*get<2>(pt) + m(2,3)*get<3>(pt);
    auto w = m(3,0)*get<0>(pt) + m(3,1)*get<1>(pt) + m(3,2)*get<2>(pt) + m(3,3)*get<3>(pt);

    return { x, y, z, w };
  }

  template<typename T, template<typename, size_t, size_t> class B, typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 2>* = nullptr, std::enable_if_t<std::is_same<coord_type_t<Point>, T>::value>* = nullptr>
  Point transform(Matrix<T, 3, 3, B> const &m, Point const &pt, T w = 1)
  {
    auto x = m(0,0)*get<0>(pt) + m(0,1)*get<1>(pt) + m(0,2)*w;
    auto y = m(1,0)*get<0>(pt) + m(1,1)*get<1>(pt) + m(1,2)*w;

    return { x, y };
  }

  template<typename T, template<typename, size_t, size_t> class B, typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr, std::enable_if_t<std::is_same<coord_type_t<Point>, T>::value>* = nullptr>
  Point transform(Matrix<T, 4, 4, B> const &m, Point const &pt, T w = 1)
  {
    auto x = m(0,0)*get<0>(pt) + m(0,1)*get<1>(pt) + m(0,2)*get<2>(pt) + m(0,3)*w;
    auto y = m(1,0)*get<0>(pt) + m(1,1)*get<1>(pt) + m(1,2)*get<2>(pt) + m(1,3)*w;
    auto z = m(2,0)*get<0>(pt) + m(2,1)*get<1>(pt) + m(2,2)*get<2>(pt) + m(2,3)*w;

    return { x, y, z };
  }

  template<typename T, template<typename, size_t, size_t> class B, typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 2>* = nullptr, std::enable_if_t<std::is_same<coord_type_t<Point>, T>::value>* = nullptr>
  Point operator *(Matrix<T, 2, 2, B> const &m, Point const &pt)
  {
    return transform(m, pt);
  }

  template<typename T, template<typename, size_t, size_t> class B, typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr, std::enable_if_t<std::is_same<coord_type_t<Point>, T>::value>* = nullptr>
  Point operator *(Matrix<T, 3, 3, B> const &m, Point const &pt)
  {
    return transform(m, pt);
  }

  template<typename T, template<typename, size_t, size_t> class B, typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 4>* = nullptr, std::enable_if_t<std::is_same<coord_type_t<Point>, T>::value>* = nullptr>
  Point operator *(Matrix<T, 4, 4, B> const &m, Point const &pt)
  {
    return transform(m, pt);
  }

  /**
   *  @}
  **/

  /**
   * \name Misc Matrix
   * \ingroup lmlmatrix
   * Matrix helpers
   * @{
  **/

  using Matrix2f = Matrix<float, 2, 2>;
  using Matrix3f = Matrix<float, 3, 3>;
  using Matrix4f = Matrix<float, 4, 4>;
  using Matrix2d = Matrix<double, 2, 2>;
  using Matrix3d = Matrix<double, 3, 3>;
  using Matrix4d = Matrix<double, 4, 4>;

  /**
   *  @}
  **/

} // namespace lml
} // namespace leap
