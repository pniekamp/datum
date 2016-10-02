//
// matrix - mathmatical matrix class
//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#ifndef LMLMATRIX_HH
#define LMLMATRIX_HH

#include "lml.h"
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
   * \b Backends \n
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

      typedef T value_type;
      typedef Backend<T, M, N> base_type;
      typedef typename base_type::data_type data_type;

    public:
      Matrix() = default;
      Matrix(std::initializer_list<T> m);
      Matrix(std::initializer_list<std::initializer_list<T>> m);
      explicit Matrix(T k);
      explicit Matrix(std::vector<std::vector<T>> const &m);

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
  Matrix<T, M, N, B>::Matrix(std::initializer_list<T> m)
  {
    auto v = m.begin();

    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        (*this)(i, j) = *v++;
  }


  //|///////////////////// Matrix::Constructor //////////////////////////////
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, M, N, B>::Matrix(std::initializer_list<std::initializer_list<T>> m)
  {
    for(size_t i = 0; i < M; ++i)
    {
      auto v = std::next(m.begin(), i)->begin();

      for(size_t j = 0; j < N; ++j)
        (*this)(i, j) = *v++;
    }
  }


  //|///////////////////// Matrix::Constructor //////////////////////////////
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, M, N, B>::Matrix(T k)
  {
    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        (*this)(i, j) = k;
  }


  //|///////////////////// Matrix::Constructor //////////////////////////////
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, M, N, B>::Matrix(std::vector<std::vector<T>> const &m)
  {
    for(size_t i = 0; i < std::min(M, m.size()); ++i)
      for(size_t j = 0; j < std::min(N, m[i].size()); ++j)
        (*this)(i, j) = m[i][j];
  }


  //|///////////////////// Matrix::Constructor //////////////////////////////
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  template<typename Q, template<typename, size_t, size_t> class C>
  Matrix<T, M, N, B>::Matrix(Matrix<Q, M, N, C> const &other)
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
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
  Matrix<T, M, N, B> scale(Matrix<T, M, N, B> const &m, T scalar)
  {
    Matrix<T, M, N, B> result;

    for(size_t i = 0; i < M; ++i)
      for(size_t j = 0; j < N; ++j)
        result(i, j) = m(i, j) * scalar;

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

    auto scale = 1/determinant(m);

    for(size_t j = 0; j < N; ++j)
    {
      for(size_t i = 0; i < N; ++i)
      {
        Matrix<T, N-1, N-1, B> sub;

        for(size_t ii = 0; ii < N-1; ++ii)
          for(size_t jj = 0; jj < N-1; ++jj)
            sub(ii, jj) = m(ii + (ii >= j), jj + (jj >= i));

        result(i, j) = (((i+j) & 1) ? -1 : 1) * determinant(sub)*scale;
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
  Matrix<T, M, N, B> operator *(S s, Matrix<T, M, N, B> const &m)
  {
    return scale(m, T(s));
  }


  //|///////////////////// operator * ///////////////////////////////////////
  /// Matrix multiplication by scalar
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B, typename S, std::enable_if_t<std::is_arithmetic<S>::value>* = nullptr>
  Matrix<T, M, N, B> operator *(Matrix<T, M, N, B> const &m, S s)
  {
    return scale(m, T(s));
  }


  //|///////////////////// operator / ///////////////////////////////////////
  /// Matrix division by scalar
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B, typename S, std::enable_if_t<std::is_arithmetic<S>::value>* = nullptr>
  Matrix<T, M, N, B> operator /(Matrix<T, M, N, B> const &m, S s)
  {
    return scale(m, 1 / T(s));
  }


  //|///////////////////// operator * ///////////////////////////////////////
  /// Matrix Multiplication by Vector
  template<typename T, size_t M, size_t N, template<typename, size_t, size_t> class B>
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


  /**
   *  @}
  **/

  /**
   * \name Misc Matrix
   * \ingroup lmlmatrix
   * Matrix helpers
   * @{
  **/

  typedef Matrix<float, 2, 2> Matrix2f;
  typedef Matrix<float, 3, 3> Matrix3f;
  typedef Matrix<float, 4, 4> Matrix4f;
  typedef Matrix<double, 2, 2> Matrix2d;
  typedef Matrix<double, 3, 3> Matrix3d;
  typedef Matrix<double, 4, 4> Matrix4d;


  /**
   *  @}
  **/

} // namespace lml
} // namespace leap

#endif
