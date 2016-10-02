//
// vector - mathmatical vector class
//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#ifndef LMLVECTOR_HH
#define LMLVECTOR_HH

#include "lml.h"
#include <functional>
#include <stdexcept>
#include <cstring>
#include <type_traits>
#include <algorithm>
#include <array>
#include <initializer_list>


/**
 * \namespace leap::lml
 * \brief Leap Math Library containing mathmatical routines
 *
**/

/**
 * \defgroup lmlvector Mathmatical Vector Class
 * \brief Vector class and mathmatical routines
 *
**/

namespace leap { namespace lml
{

  //|-------------------- VectorView  ---------------------------------------
  //|------------------------------------------------------------------------

  template<typename Vector, typename T, size_t... Indices>
  class VectorView
  {
    public:

      typedef T value_type;
      typedef Vector vector_type;
      typedef index_sequence<Indices...> indices_type;

      static constexpr size_t size() { return sizeof...(Indices); }

      Vector operator()() const { return { (*this)[Indices]... }; }

      VectorView &operator =(Vector const &v) { std::tie((*this)[Indices]...) = tie(&v[0], make_index_sequence<0, sizeof...(Indices)>()); return *this; }

      template<typename V>
      VectorView &operator +=(V &&v) { *this = *this + std::forward<V>(v); return *this; }

      template<typename V>
      VectorView &operator -=(V &&v) { *this = *this - std::forward<V>(v); return *this; }

      template<typename V>
      VectorView &operator *=(V &&v) { *this = *this * std::forward<V>(v); return *this; }

      template<typename V>
      VectorView &operator /=(V &&v) { *this = *this / std::forward<V>(v); return *this; }

      constexpr T const &operator[](size_t i) const { return *((T const *)this + i); }
      T &operator[](size_t i) { return *((T*)this + i); }

    protected:
      VectorView() = default;
      VectorView &operator =(VectorView const &) = default;
  };


  //|///////////////////// VectorView get ///////////////////////////////////
  template<size_t i, typename Vector, typename T, size_t... Indices>
  constexpr auto const &get(VectorView<Vector, T, Indices...> const &v) noexcept
  {
    return v[get<i>(index_sequence<Indices...>())];
  }


  // vector_view_for
  template<typename Vector, typename T, typename IndexSequence>
  struct vector_view_for
  {
    template <typename>
    struct detail;

    template <size_t... Indices>
    struct detail<index_sequence<Indices...>>
    {
      using type = VectorView<Vector, T, Indices...>;
    };

    using type = typename detail<IndexSequence>::type;
  };

  template<typename Vector, typename T, typename IndexSequence>
  using vector_view_for_t = typename vector_view_for<Vector, T, IndexSequence>::type;

  // vector_view_to
  template<typename U>
  struct vector_view_to
  {
    template<typename T, size_t... Indices>
    static constexpr U from(T const &v, index_sequence<Indices...>)
    {
      return { get<Indices>(v)... };
    }
  };

  // is_vector_view
  template<typename T, typename = void>
  struct is_vector_view : std::false_type { };

  template<typename T>
  struct is_vector_view<T, std::enable_if_t<std::is_base_of<vector_view_for_t<typename T::vector_type, typename T::value_type, typename T::indices_type>, T>::value>> : std::true_type { };


  //|-------------------- Vector --------------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup lmlvector
   *
   * \brief Mathmatical Vector Class
   *
   * This vector class wraps the use of a mathmatical vector of N-N.
   *
   * Example Usage :
   * \code
   *
   *   #include <leap/lml/io.h>
   *   #include <leap/lml/vector.h>
   *
   *   using namespace leap::lml;
   *
   *   void Test()
   *   {
   *     Vector3f A;
   *     A(0) = 1.1;
   *     A(1) = 5.4;
   *     A(2) = -3.2;
   *
   *     cout << A << endl;
   *   }
   * \endcode
   *
   * \see lmlvector
  **/

  template<typename T, size_t N>
  class Vector : public vector_view_for_t<Vector<T, N>, T, make_index_sequence<0, N>>
  {
    public:

      typedef T value_type;
      typedef std::array<T, N> data_type;

    public:
      Vector() = default;
      constexpr Vector(std::initializer_list<T> v);
      explicit constexpr Vector(T k);
      explicit constexpr Vector(std::array<T, N> const &v);
      explicit Vector(std::vector<T> const &v);

      // Element Access
      constexpr T const &operator()(size_t i) const { return m_data[i]; }
      T &operator()(size_t i) { return m_data[i]; }

      // Storage Access
      constexpr data_type const &data() const { return m_data; }
      data_type &data() { return m_data; }

      // Converters
      template<typename U> U to() const;
      template<typename U> static Vector from(U const &other);

    private:

      std::array<T, N> m_data;
  };


  //|///////////////////// Vector::Constructor //////////////////////////////
  template<typename T, size_t N>
  constexpr Vector<T, N>::Vector(std::initializer_list<T> v)
    : m_data(gather(v.begin(), make_index_sequence<0, N>()))
  {
  }


  //|///////////////////// Vector::Constructor //////////////////////////////
  template<typename T, size_t N>
  constexpr Vector<T, N>::Vector(T k)
    : m_data(fill(k, make_index_sequence<0, N>()))
  {
  }


  //|///////////////////// Vector::Constructor //////////////////////////////
  template<typename T, size_t N>
  constexpr Vector<T, N>::Vector(std::array<T, N> const &v)
    : m_data(v)
  {
  }


  //|///////////////////// Vector::Constructor //////////////////////////////
  template<typename T, size_t N>
  Vector<T, N>::Vector(std::vector<T> const &v)
  {
    for(size_t i = 0; i < std::min(N, v.size()); ++i)
      (*this)(i) = v[i];
  }


  //|///////////////////// Vector::to ///////////////////////////////////////
  template<typename T, size_t N>
  template<typename U>
  U Vector<T, N>::to() const
  {
    return vector_view_to<U>::from(*this, make_index_sequence<0, N>());
  }


  //|///////////////////// Vector::from /////////////////////////////////////
  template<typename T, size_t N>
  template<typename U>
  Vector<T, N> Vector<T, N>::from(U const &other)
  {
    return vector_view_to<Vector<T, N>>::from(other, make_index_sequence<0, N>());
  }


  //|///////////////////// Vector operator == ///////////////////////////////
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices>
  constexpr bool operator ==(VectorView<Vector, T, Indices...> const &lhs, VectorView<Vector, T, Jndices...> const &rhs)
  {
    return foldl<std::logical_and<bool>>((lhs[Indices] == rhs[Jndices])...);
  }


  //|///////////////////// Vector operator != ///////////////////////////////
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices>
  constexpr bool operator !=(VectorView<Vector, T, Indices...> const &lhs, VectorView<Vector, T, Jndices...> const &rhs)
  {
    return !(lhs == rhs);
  }



  /**
   * \name General Vector Operations
   * \ingroup lmlvector
   * Generic operations for vectors of any backend
   * @{
  **/


  //|///////////////////// normsqr /////////////////////////////////////////////
  /// length (norm) of the vector squared
  template<typename Vector, typename T, size_t... Indices>
  constexpr T normsqr(VectorView<Vector, T, Indices...> const &v)
  {
    return foldl<std::plus<T>>((v[Indices] * v[Indices])...);
  }


  //|///////////////////// norm /////////////////////////////////////////////
  /// length (norm) of the vector
  template<typename Vector, typename T, size_t... Indices>
  constexpr T norm(VectorView<Vector, T, Indices...> const &v)
  {
    using std::sqrt;

    return sqrt(normsqr(v));
  }


  //|///////////////////// scale ////////////////////////////////////////////
  /// scales a vector
  template<typename Vector, typename T, size_t... Indices>
  constexpr Vector scale(VectorView<Vector, T, Indices...> const &v, T scalar)
  {
    return { (v[Indices] * scalar)... };
  }


  //|///////////////////// normalise ////////////////////////////////////////
  /// normalise a vector to a unit vector
  template<typename Vector, typename T, size_t... Indices>
  constexpr Vector normalise(VectorView<Vector, T, Indices...> const &v)
  {
    return scale(v, 1/norm(v));
  }


  //|///////////////////// safenormalise ////////////////////////////////////
  /// normalise a vector to a unit vector or return zero
  template<typename Vector, typename T, size_t... Indices>
  constexpr Vector safenormalise(VectorView<Vector, T, Indices...> const &v)
  {
    auto lengthsqr = normsqr(v);

    return fcmp(lengthsqr, T(0)) ? Vector(T(0)) : scale(v, 1/std::sqrt(lengthsqr));
  }


  //|///////////////////// abs //////////////////////////////////////////////
  /// elementwise abs
  template<typename Vector, typename T, size_t... Indices>
  constexpr Vector abs(VectorView<Vector, T, Indices...> const &v)
  {
    using std::abs;
  
    return { abs(v[Indices])... };
  }


  //|///////////////////// min //////////////////////////////////////////////
  /// elementwise min
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices>
  constexpr Vector min(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v)
  {
    using std::min;

    return { min(u[Indices], v[Jndices])... };
  }


  //|///////////////////// max //////////////////////////////////////////////
  /// elementwise max
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices>
  constexpr Vector max(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v)
  {
    using std::max;
    
    return { max(u[Indices], v[Jndices])... };
  }


  //|///////////////////// floor ////////////////////////////////////////////
  /// elementwise floor
  template<typename Vector, typename T, size_t... Indices>
  constexpr Vector floor(VectorView<Vector, T, Indices...> const &v)
  {
    using std::floor;

    return { floor(v[Indices])... };
  }


  //|///////////////////// ceil /////////////////////////////////////////////
  /// elementwise ceil
  template<typename Vector, typename T, size_t... Indices>
  constexpr Vector ceil(VectorView<Vector, T, Indices...> const &v)
  {
    using std::ceil;

    return { ceil(v[Indices])... };
  }


  //|///////////////////// clamp ////////////////////////////////////////////
  /// elementwise clamp
  template<typename Vector, typename T, size_t... Indices>
  constexpr Vector clamp(VectorView<Vector, T, Indices...> const &v, T lower, T upper)
  {
    return { clamp(v[Indices], lower, upper)... };
  }


  //|///////////////////// lerp /////////////////////////////////////////////
  /// elementwise lerp
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices>
  constexpr Vector lerp(VectorView<Vector, T, Indices...> const &lower, VectorView<Vector, T, Jndices...> const &upper, T alpha)
  {  
    return { lerp(lower[Indices], upper[Jndices], alpha)... };
  }


  //|///////////////////// min_element //////////////////////////////////////
  /// min element
  template<typename Vector, typename T, size_t... Indices>
  constexpr T min_element(VectorView<Vector, T, Indices...> const &v)
  {
    using std::min;

    return min({ v[Indices]... });
  }


  //|///////////////////// max_element //////////////////////////////////////
  /// max element
  template<typename Vector, typename T, size_t... Indices>
  constexpr T max_element(VectorView<Vector, T, Indices...> const &v)
  {
    using std::max;

    return max({ v[Indices]... });
  }


  //|///////////////////// perp /////////////////////////////////////////////
  /// perp operator
  template<typename Vector, typename T, size_t... Indices, std::enable_if_t<sizeof...(Indices) == 2>* = nullptr>
  constexpr Vector perp(VectorView<Vector, T, Indices...> const &v)
  {
    return { -get<1>(v), get<0>(v) };
  }


  //|///////////////////// perp /////////////////////////////////////////////
  /// perp product
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices, std::enable_if_t<sizeof...(Indices) == 2>* = nullptr>
  constexpr T perp(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v)
  {
    return get<0>(u) * get<1>(v) - get<1>(u) * get<0>(v);
  }


  //|///////////////////// dot //////////////////////////////////////////////
  /// dot product
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices>
  constexpr T dot(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v)
  {
    return foldl<std::plus<T>>((u[Indices] * v[Jndices])...);
  }


  //|///////////////////// cross ////////////////////////////////////////////
  /// cross product
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices, std::enable_if_t<sizeof...(Indices) == 3>* = nullptr>
  constexpr Vector cross(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v)
  {
    return { get<1>(u) * get<2>(v) - get<2>(u) * get<1>(v), get<2>(u) * get<0>(v) - get<0>(u) * get<2>(v), get<0>(u) * get<1>(v) - get<1>(u) * get<0>(v) };
  }


  //|///////////////////// hada /////////////////////////////////////////////
  /// hadamard product
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices>
  constexpr Vector hada(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v)
  {
    return { (u[Indices] * v[Jndices])... };
  }


  //|///////////////////// orthogonal ///////////////////////////////////////
  /// orthogonal vector
  template<typename Vector, typename T, size_t... Indices, std::enable_if_t<sizeof...(Indices) == 3>* = nullptr>
  Vector orthogonal(VectorView<Vector, T, Indices...> const &u)
  {
    float x = std::abs(get<0>(u));
    float y = std::abs(get<1>(u));
    float z = std::abs(get<2>(u));

    return cross(u, x < y ? (x < z ? Vector{T(1), T(0), T(0)} : Vector{T(0), T(0), T(1)}) : (y < z ? Vector{T(0), T(1), T(0)} : Vector{T(0), T(0), T(1)}));
  }


  //|///////////////////// orthogonal ///////////////////////////////////////
  /// orthogonal vector
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices, std::enable_if_t<sizeof...(Indices) == 3>* = nullptr>
  Vector orthogonal(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v)
  {
    auto axis = cross(u, v);

    if (fcmp(normsqr(axis), T(0)))
    {
      axis = orthogonal(u);
    }

    return axis;
  }


  //|///////////////////// orthonormalise ////////////////////////////////////////////
  /// orthogonalise & normalise u, v, generate w
  template<typename Vector>
  void orthonormalise(Vector &u, Vector &v, Vector &w)
  {
    w = orthogonal(u, v);
    u = normalise(u - w * dot(w, u));
    v = normalise(cross(w, u));
    w = cross(u, v);
  }


  //|///////////////////// theta ////////////////////////////////////////////
  /// spherical coordinates azimuthal angle of the vector
  template<typename Vector, typename T, size_t... Indices>
  constexpr T theta(VectorView<Vector, T, Indices...> const &v)
  {
    return std::atan2(get<1>(v), get<0>(v));
  }


  //|///////////////////// phi /////////////////////////////////////////////
  /// spherical coordinates polar angle of the vector
  template<typename Vector, typename T, size_t... Indices>
  constexpr T phi(VectorView<Vector, T, Indices...> const &v)
  {
    return std::acos(get<2>(v)/norm(v));
  }


  //|///////////////////// theta ////////////////////////////////////////////
  /// angle between two unit vectors (unsigned)
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices>
  constexpr T theta(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v)
  {
    return std::acos(clamp<T>(dot(u, v), -1, 1));
  }


  //|///////////////////// theta ////////////////////////////////////////////
  /// angle between two unit vectors (signed)
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices, std::enable_if_t<sizeof...(Indices) == 2>* = nullptr>
  constexpr T theta(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v, T normal)
  {
    return std::copysign(theta(u, v), normal * perp(u, v));
  }


  //|///////////////////// theta ////////////////////////////////////////////
  /// angle between two unit vectors (signed)
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices, size_t... Kndices, std::enable_if_t<sizeof...(Indices) == 3>* = nullptr>
  constexpr T theta(VectorView<Vector, T, Indices...> const &u, VectorView<Vector, T, Jndices...> const &v, VectorView<Vector, T, Kndices...> const &normal)
  {
    return std::copysign(theta(u, v), dot(normal, cross(u, v)));
  }


  //|///////////////////// operator - ///////////////////////////////////////
  /// Vector Subtraction
  template<typename Vector, typename T, size_t... Indices>
  constexpr Vector operator -(VectorView<Vector, T, Indices...> const &v)
  {
    return scale(v, T(-1));
  }


  //|///////////////////// operator + ///////////////////////////////////////
  /// Vector Addition
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices>
  constexpr Vector operator +(VectorView<Vector, T, Indices...> const &v1, VectorView<Vector, T, Jndices...> const &v2)
  {
    return { (v1[Indices] + v2[Jndices])... };
  }


  //|///////////////////// operator - ///////////////////////////////////////
  /// Vector Subtraction
  template<typename Vector, typename T, size_t... Indices, size_t... Jndices>
  constexpr Vector operator -(VectorView<Vector, T, Indices...> const &v1, VectorView<Vector, T, Jndices...> const &v2)
  {
    return { (v1[Indices] - v2[Jndices])... };
  }


  //|///////////////////// operator * ///////////////////////////////////////
  /// Vector multiplication by scalar
  template<typename Vector, typename T, size_t... Indices, typename S, std::enable_if_t<std::is_arithmetic<S>::value>* = nullptr>
  constexpr Vector operator *(S s, VectorView<Vector, T, Indices...> const &v)
  {
    return scale(v, T(s));
  }


  //|///////////////////// operator * ///////////////////////////////////////
  /// Vector multiplication by scalar
  template<typename Vector, typename T, size_t... Indices, typename S, std::enable_if_t<std::is_arithmetic<S>::value>* = nullptr>
  constexpr Vector operator *(VectorView<Vector, T, Indices...> const &v, S s)
  {
    return scale(v, T(s));
  }


  //|///////////////////// operator / ///////////////////////////////////////
  /// Vector division by scalar
  template<typename Vector, typename T, size_t... Indices, typename S, std::enable_if_t<std::is_arithmetic<S>::value>* = nullptr>
  constexpr Vector operator /(VectorView<Vector, T, Indices...> const &v, S s)
  {
    return scale(v, 1 / T(s));
  }


  //|///////////////////// swizzle //////////////////////////////////////////
  /// swizzle
  template<size_t... Elements, typename Vector, typename T, size_t... Indices>
  constexpr Vector swizzle(VectorView<Vector, T, Indices...> const &v)
  {
    return { get<Elements>(v)... };
  }


  /**
   *  @}
  **/

  /**
   * \name Misc Vector
   * \ingroup lmlvector
   * Vector helpers
   * @{
  **/

  typedef Vector<float, 2> Vector2f;
  typedef Vector<float, 3> Vector3f;
  typedef Vector<float, 4> Vector4f;
  typedef Vector<double, 2> Vector2d;
  typedef Vector<double, 3> Vector3d;
  typedef Vector<double, 4> Vector4d;


  //|///////////////////// Vector2 //////////////////////////////////////////
  /// Creates a simple 2 element vector

  template<typename T>
  constexpr Vector<T, 2> Vector2(T k)
  {
    return Vector<T, 2>(std::array<T, 2>{ k, k });
  }

  template<typename T>
  constexpr Vector<T, 2> Vector2(T x, T y)
  {
    return Vector<T, 2>(std::array<T, 2>{ x, y });
  }


  //|///////////////////// Polar2 ///////////////////////////////////////////
  /// Creates a simple 2 element vector (polar)
  template<typename T>
  constexpr Vector<T, 2> Polar2(T angle, T length)
  {
    return Vector<T, 2>(std::array<T, 2>{ std::cos(angle)*length, std::sin(angle)*length });
  }


  //|///////////////////// Vector3 //////////////////////////////////////////
  /// Creates a simple 3 element vector

  template<typename T>
  constexpr Vector<T, 3> Vector3(T k)
  {
    return Vector<T, 3>(std::array<T, 3>{ k, k, k });
  }

  template<typename T>
  constexpr Vector<T, 3> Vector3(T x, T y, T z)
  {
    return Vector<T, 3>(std::array<T, 3>{ x, y, z });
  }

  template<typename T>
  constexpr Vector<T, 3> Vector3(Vector<T, 2> const &v, T z)
  {
    return Vector<T, 3>(std::array<T, 3>{ v(0), v(1), z });
  }


  //|///////////////////// Vector4 //////////////////////////////////////////
  /// Creates a simple 4 element vector

  template<typename T>
  constexpr Vector<T, 4> Vector4(T k)
  {
    return Vector<T, 4>(std::array<T, 4>{ k, k, k, k });
  }

  template<typename T>
  constexpr Vector<T, 4> Vector4(T x, T y, T z, T w)
  {
    return Vector<T, 4>(std::array<T, 4>{ x, y, z, w });
  }

  template<typename T>
  constexpr Vector<T, 4> Vector4(Vector<T, 3> const &v, T w)
  {
    return Vector<T, 4>(std::array<T, 4>{ v(0), v(1), v(2), w });
  }



  //|///////////////////// Vector Constants /////////////////////////////////

  Vector2f const xUnit2f = Vector2(1.0f, 0.0f);
  Vector2f const yUnit2f = Vector2(0.0f, 1.0f);

  Vector2d const xUnit2d = Vector2(1.0, 0.0);
  Vector2d const yUnit2d = Vector2(0.0, 1.0);

  Vector3f const xUnit3f = Vector3(1.0f, 0.0f, 0.0f);
  Vector3f const yUnit3f = Vector3(0.0f, 1.0f, 0.0f);
  Vector3f const zUnit3f = Vector3(0.0f, 0.0f, 1.0f);

  Vector3d const xUnit3d = Vector3(1.0, 0.0, 0.0);
  Vector3d const yUnit3d = Vector3(0.0, 1.0, 0.0);
  Vector3d const zUnit3d = Vector3(0.0, 0.0, 1.0);


  /**
   *  @}
  **/


} // namespace lml
} // namespace leap

#endif
