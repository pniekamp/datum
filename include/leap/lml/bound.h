//
// bound
//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#ifndef BOUND_HH
#define BOUND_HH

#include "lml.h"
#include "point.h"
#include <leap/lml/vector.h>
#include <leap/lml/matrix.h>
#include <leap/optional.h>
#include <cstddef>
#include <functional>
#include <vector>
#include <limits>

/**
 * \namespace leap::lml
 * \brief Leap Math Library containing mathmatical routines
 *
**/

/**
 * \defgroup leapdata Data Containors
 * \brief Data Containors
 *
**/

namespace leap { namespace lml
{

  //|-------------------- BoundView  ----------------------------------------
  //|------------------------------------------------------------------------

  template<typename Bound, typename T, size_t Stride, size_t... Indices>
  class BoundView
  {
    public:

      typedef T value_type;
      typedef index_sequence<Indices...> indices_type;
      typedef Bound bound_type;

      static constexpr size_t size() { return sizeof...(Indices); }
      static constexpr size_t stride() { return Stride; }

      Bound operator()() const { return { { (*this)[0][Indices]... }, { (*this)[1][Indices]... } }; }

      BoundView &operator =(Bound const &b) { for(int i = 0; i < 2; ++ i) std::tie((*this)[i][Indices]...) = tie(b[i], make_index_sequence<0, sizeof...(Indices)>()); return *this; }

      constexpr T const *operator[](size_t j) const { return ((T*)this + j*Stride); }
      T *operator[](size_t j) { return ((T*)this + j*Stride); }

    protected:
      BoundView() = default;
      BoundView &operator =(BoundView const &) = default;
  };

  //|///////////////////// BoundView low ////////////////////////////////////
  template<size_t i, typename Bound, typename T, size_t Stride, size_t... Indices>
  constexpr auto const &low(BoundView<Bound, T, Stride, Indices...>  const &bound) noexcept
  {
    return bound[0][get<i>(index_sequence<Indices...>())];
  }

  //|///////////////////// BoundView high ///////////////////////////////////
  template<size_t i, typename Bound, typename T, size_t Stride, size_t... Indices>
  constexpr auto const &high(BoundView<Bound, T, Stride, Indices...>  const &bound) noexcept
  {
    return bound[1][get<i>(index_sequence<Indices...>())];
  }

  // bound_view_for
  template<typename Bound, typename T, typename IndexSequence>
  struct bound_view_for
  {
    template <typename>
    struct detail;

    template <size_t... Indices>
    struct detail<index_sequence<Indices...>>
    {
      using type = BoundView<Bound, T, sizeof...(Indices), Indices...>;
    };

    using type = typename detail<IndexSequence>::type;
  };

  template<typename Bound, typename T, typename IndexSequence>
  using bound_view_for_t = typename bound_view_for<Bound, T, IndexSequence>::type;



  //|-------------------- Bound ---------------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup leapdata
   *
   * \brief Arbitary Dimension Bounding Box (Axis Aligned)
   *
  **/

  template<typename T, size_t N>
  class Bound : public bound_view_for_t<Bound<T, N>, T, make_index_sequence<0, N>>
  {
    public:

      typedef T value_type;

    public:
      Bound() = default;
      constexpr Bound(std::initializer_list<T> lo, std::initializer_list<T> hi);
      explicit constexpr Bound(std::array<T, N> const &lo, std::array<T, N> const &hi);

      constexpr T centre(size_t axis) const { return (m_lo[axis] + m_hi[axis])/2; }
      constexpr T halfdim(size_t axis) const { return (m_hi[axis] - m_lo[axis])/2; }

      constexpr T low(size_t axis) const { return m_lo[axis]; }
      constexpr T high(size_t axis) const { return m_hi[axis]; }

      void set(size_t axis, T low, T high)
      {
        m_lo[axis] = low;
        m_hi[axis] = high;
      }

    private:

      std::array<T, N> m_lo;
      std::array<T, N> m_hi;
  };


  //|///////////////////// Bound::Constructor ///////////////////////////////
  template<typename T, size_t N>
  constexpr Bound<T, N>::Bound(std::initializer_list<T> lo, std::initializer_list<T> hi)
    : m_lo((lo.size() == N) ? gather(lo.begin(), make_index_sequence<0, N>()) : throw 0),
      m_hi((hi.size() == N) ? gather(hi.begin(), make_index_sequence<0, N>()) : throw 0)
  {
  }


  //|///////////////////// Bound::Constructor ///////////////////////////////
  template<typename T, size_t N>
  constexpr Bound<T, N>::Bound(std::array<T, N> const &lo, std::array<T, N> const &hi)
    : m_lo(lo),
      m_hi(hi)
  {
  }


  //|///////////////////// make_bound ///////////////////////////////////////
  template<typename Bound, typename Point, size_t... Indices>
  auto make_bound(Point const &lo, Point const &hi, index_sequence<Indices...>)
  {
    return Bound({ get<Indices>(lo)... }, { get<Indices>(hi)... });
  }

  template<typename Bound, typename Point, size_t dimension = dim<Point>()>
  auto make_bound(Point const &lo, Point const &hi)
  {
    return make_bound<Bound>(lo, hi, make_index_sequence<0, dimension>());
  }

  template<typename Point, size_t dimension = dim<Point>()>
  auto make_bound(Point const &lo, Point const &hi)
  {
    return make_bound<Bound<coord_type_t<Point>, dimension>>(lo, hi, make_index_sequence<0, dimension>());
  }

  template<typename Bound, typename Point, size_t dimension = dim<Point>()>
  auto make_bound(Point const &centre, std::decay_t<decltype(get<0>(std::declval<Point&>()))> halfdim)
  {
    auto lo = translate(centre, Vector<coord_type_t<Point>, dimension>(-halfdim));
    auto hi = translate(centre, Vector<coord_type_t<Point>, dimension>(+halfdim));

    return make_bound<Bound>(lo, hi);
  }

  template<typename Point, size_t dimension = dim<Point>()>
  auto make_bound(Point const &centre, std::decay_t<decltype(get<0>(std::declval<Point&>()))> halfdim)
  {
    return make_bound<Bound<coord_type_t<Point>, dimension>>(centre, halfdim);
  }


  //|///////////////////// bound_limits /////////////////////////////////////
  template<typename Bound>
  struct bound_limits
  {
    typedef typename Bound::value_type T;
    static constexpr size_t N = Bound::size();

    //|///////////////////// min ////////////////////////////////////////////
    static constexpr Bound min()
    {
      return make_bound<Bound>(fill(std::numeric_limits<T>::max(), make_index_sequence<0, N>()), fill(std::numeric_limits<T>::lowest(), make_index_sequence<0, N>()));
    }

    //|///////////////////// max ////////////////////////////////////////////
    static constexpr Bound max()
    {
      return make_bound<Bound>(fill(std::numeric_limits<T>::lowest(), make_index_sequence<0, N>()), fill(std::numeric_limits<T>::max(), make_index_sequence<0, N>()));
    }
  };


  //|///////////////////// Bound::operator == ///////////////////////////////
  template<typename Bound, typename T, size_t IStride, size_t... Indices, size_t JStride, size_t... Jndices>
  constexpr bool operator ==(BoundView<Bound, T, IStride, Indices...> const &lhs, BoundView<Bound, T, JStride, Jndices...> const &rhs)
  {
    return foldl<std::logical_and<bool>>((lhs[0][Indices] == rhs[0][Jndices] && lhs[1][Indices] == rhs[1][Jndices])...);
  }


  //|///////////////////// Bound::operator != ///////////////////////////////
  template<typename Bound, typename T, size_t... Indices, size_t... Jndices>
  constexpr bool operator !=(BoundView<Bound, T, Indices...> const &lhs, BoundView<Bound, T, Jndices...> const &rhs)
  {
    return !(lhs == rhs);
  }


  /**
   * \name Bound Operations
   * \ingroup lmlbound
   * General operations on Bounds
   * @{
  **/

  //|///////////////////// expand ///////////////////////////////////////////
  /// Expansion of two bounds (union)
  template<typename Bound, typename T, size_t IStride, size_t... Indices, size_t JStride, size_t... Jndices>
  auto expand(BoundView<Bound, T, IStride, Indices...> const &b1, BoundView<Bound, T, JStride, Jndices...> const &b2)
  {
    return Bound({ std::min(b1[0][Indices], b2[0][Jndices])... }, { std::max(b1[1][Indices], b2[1][Jndices])... });
  }


  //|///////////////////// expand //////////////////////////////////////////
  /// Expand bound to include point
  template<typename Bound, typename T, size_t Stride, size_t... Indices, typename Point, size_t... Kndices>
  auto expand(BoundView<Bound, T, Stride, Indices...> const &bound, Point const &pt, index_sequence<Kndices...>)
  {
    return Bound({ std::min(bound[0][Indices], get<Kndices>(pt))... }, { std::max(bound[1][Indices], get<Kndices>(pt))... });
  }

  template<typename Bound, typename T, size_t Stride, size_t... Indices, typename Point, size_t dimension = dim<Point>()>
  auto expand(BoundView<Bound, T, Stride, Indices...> const &bound, Point const &pt)
  {
    return expand(bound, pt, make_index_sequence<0, dimension>());
  }


  //|///////////////////// intersects ///////////////////////////////////////
  /// Test if intersects
  template<typename Bound, typename T, size_t IStride, size_t... Indices, size_t JStride, size_t... Jndices>
  bool intersects(BoundView<Bound, T, IStride, Indices...> const &b1, BoundView<Bound, T, JStride, Jndices...> const &b2)
  {
    return foldl<std::logical_and<bool>>((b1[0][Indices] <= b2[1][Jndices] && b1[1][Indices] >= b2[0][Jndices])...);
  }


  //|///////////////////// intersection /////////////////////////////////////
  /// intersection of two bounds
  template<typename Bound, typename T, size_t IStride, size_t... Indices, size_t JStride, size_t... Jndices>
  auto intersection(BoundView<Bound, T, IStride, Indices...> const &b1, BoundView<Bound, T, JStride, Jndices...> const &b2)
  {
    leap::optional<Bound> result;

    if (intersects(b1, b2))
    {
      result = Bound({ std::max(b1[0][Indices], b2[0][Jndices])... }, { std::min(b1[1][Indices], b2[1][Jndices])... });
    }

    return result;
  }


  //|///////////////////// intersection /////////////////////////////////////
  /// intersection of bound and line segment
  template<typename Point>
  struct slabintersect : public leap::optional<Point>
  {
    coord_type_t<Point> tmin;
    coord_type_t<Point> tmax;

    bool ray() const { return tmax > std::max(tmin, coord_type_t<Point>(0)); }
    bool seg() const { return tmax > std::max(tmin, coord_type_t<Point>(0)) && tmin < coord_type_t<Point>(1); }
  };

  template<typename Bound, typename Point, size_t... Indices>
  auto intersection(Bound const &bound, Point const &a, Point const &b, index_sequence<Indices...>)
  {
    using T = coord_type_t<Point>;

    slabintersect<Point> result;

    auto t1 = std::array<T, sizeof...(Indices)>{ ((low<Indices>(bound) - get<Indices>(a)) / (get<Indices>(b) - get<Indices>(a)))... };
    auto t2 = std::array<T, sizeof...(Indices)>{ ((high<Indices>(bound) - get<Indices>(a)) / (get<Indices>(b) - get<Indices>(a)))... };

    result.tmin = std::min(t1[0], t2[0]);
    result.tmax = std::max(t1[0], t2[0]);

    for(size_t i = 1; i < bound.size(); ++i)
    {
      result.tmin = std::max(result.tmin, std::min(std::min(t1[i], t2[i]), result.tmax));
      result.tmax = std::min(result.tmax, std::max(std::max(t1[i], t2[i]), result.tmin));
    }

    if (result.tmax > result.tmin)
    {
      result.emplace(a + (result.tmin < 0 ? result.tmax : result.tmin) * vec(a, b));
    }

    return result;
  }

  template<typename Bound, typename T, size_t IStride, size_t... Indices, typename Point>
  auto intersection(BoundView<Bound, T, IStride, Indices...> const &bound, Point const &a, Point const &b)
  {
    return intersection(bound, a, b, make_index_sequence<0, sizeof...(Indices)>());
  }


  //|///////////////////// contains /////////////////////////////////////////
  /// bound contains point
  template<typename Bound, typename T, size_t Stride, size_t... Indices, typename Point, size_t... Kndices>
  bool contains(BoundView<Bound, T, Stride, Indices...> const &bound, Point const &pt, index_sequence<Kndices...>)
  {
    return foldl<std::logical_and<bool>>((bound[0][Indices] <= get<Kndices>(pt) && get<Kndices>(pt) <= bound[1][Indices])...);
  }

  template<typename Bound, typename T, size_t Stride, size_t... Indices, typename Point, size_t dimension = dim<Point>()>
  bool contains(BoundView<Bound, T, Stride, Indices...> const &bound, Point const &pt)
  {
    return contains(bound, pt, make_index_sequence<0, dimension>());
  }


  //|///////////////////// contains /////////////////////////////////////////
  /// bound contains bound
  template<typename Bound, typename T, size_t IStride, size_t... Indices, size_t JStride, size_t... Jndices>
  bool contains(BoundView<Bound, T, IStride, Indices...> const &b1, BoundView<Bound, T, JStride, Jndices...> const &b2)
  {
    return foldl<std::logical_and<bool>>((b2[0][Jndices] >= b1[0][Indices] && b2[1][Jndices] <= b1[1][Indices])...);
  }


  //|///////////////////// volume ///////////////////////////////////////////
  /// volume of bound
  template<typename Bound, typename T, size_t Stride, size_t... Indices>
  auto volume(BoundView<Bound, T, Stride, Indices...> const &bound)
  {
    return foldl<std::multiplies<T>>(T(1), (bound[1][Indices] - bound[0][Indices])...);
  }


  //|///////////////////// nearest_in_bound /////////////////////////////////
  /// nearest point on or within bound
  template<typename Bound, typename T, size_t Stride, size_t... Indices, typename Point, std::enable_if_t<dim<Point>() == sizeof...(Indices)>* = nullptr>
  Point nearest_in_bound(BoundView<Bound, T, Stride, Indices...> const &bound, Point const &pt)
  {
    return { clamp(get<Indices>(pt), bound[0][Indices], bound[1][Indices])... };
  }


  //|///////////////////// transform ////////////////////////////////////////
  /// transform bounding box (affine transform only)
  template<typename T, size_t N, template<typename, size_t, size_t> class B, typename Bound, size_t Stride, size_t... Indices, std::enable_if_t<sizeof...(Indices) == N-1>* = nullptr>
  auto transform(Matrix<T, N, N, B> const &matrix, BoundView<Bound, T, Stride, Indices...> const &bound)
  {
    auto centre = transform(matrix, Vector<T, N-1>{ ((bound[0][Indices] + bound[1][Indices])/2)... });

    auto halfdim = transform(abs(matrix), Vector<T, N-1>{ ((bound[1][Indices] - bound[0][Indices])/2)... }, T(0));

    return make_bound<Bound>(centre - halfdim, centre + halfdim);
  }


  //|///////////////////// make_bound ///////////////////////////////////////
  /// bound around points
  template<typename InputIterator, size_t dimension = dim<typename std::iterator_traits<InputIterator>::value_type>()>
  auto make_bound(InputIterator f, InputIterator l)
  {
    auto result = bound_limits<decltype(make_bound(*f, 0))>::min();

    for(InputIterator i = f; i != l; ++i)
    {
      result = expand(result, *i);
    }

    return result;
  }


  /**
   *  @}
  **/

  /**
   * \name Misc Bound
   * \ingroup lmlbound
   * Bound helpers
   * @{
  **/

  typedef Bound<float, 2> Bound2f;
  typedef Bound<float, 3> Bound3f;
  typedef Bound<double, 2> Bound2d;
  typedef Bound<double, 3> Bound3d;


  /**
   *  @}
  **/

} // namespace lml
} // namespace leap

#endif
