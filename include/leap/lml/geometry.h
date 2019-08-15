//
// geometry - basic point and line geometry routines
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
#include <leap/lml/matrix.h>
#include <leap/optional.h>
#include <vector>
#include <list>
#include <utility>
#include <algorithm>

/**
 * \namespace leap::lml
 * \brief Leap Math Library containing mathmatical routines
 *
**/

/**
 * \defgroup lmlgeo Geometry
 * \brief Geometry Routines
 *
**/

namespace leap { namespace lml
{
  //|///////////////////// pi ///////////////////////////////////////////////
  template<typename T>
  constexpr T pi() { return T(3.141592653589793238462643383279502884); }


  //|///////////////////// distsqr //////////////////////////////////////////
  /// distance between two points squared
  template<typename Point, size_t dimension = dim<Point>()>
  constexpr auto distsqr(Point const &a, Point const &b)
  {
    return normsqr(vec(a, b));
  }


  //|///////////////////// dist /////////////////////////////////////////////
  /// distance between two points
  template<typename Point, size_t dimension = dim<Point>()>
  constexpr auto dist(Point const &a, Point const &b)
  {
    return std::sqrt(distsqr(a, b));
  }


  //|///////////////////// area /////////////////////////////////////////////
  /// area of triangle
  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 2>* = nullptr>
  constexpr auto area(Point const &a, Point const &b, Point const &c)
  {
    return std::abs(perp(vec(a, b), vec(a, c))) / 2;
  }

  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr>
  constexpr auto area(Point const &a, Point const &b, Point const &c)
  {
    return norm(cross(vec(a, b), vec(a, c))) / 2;
  }


  //|///////////////////// coincident ///////////////////////////////////////
  /// coincident points
  template<typename Point>
  constexpr bool coincident(Point const &a, Point const &b)
  {
    return fcmp(distsqr(a, b), decltype(dist(a, b))(0));
  }


  //|///////////////////// collinear ////////////////////////////////////////
  /// collinear points
  template<typename Point>
  constexpr bool collinear(Point const &a, Point const &b, Point const &c)
  {
    return fcmp(area(a, b, c), decltype(area(a, b, c))(0));
  }


  //|///////////////////// orientation //////////////////////////////////////
  /// orientation of xy triangle, clockwise < 0, anticlockwise > 0
  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 2>* = nullptr>
  auto orientation(Point const &a, Point const &b, Point const &c)
  {
    auto result = perp(vec(a, b), vec(a, c));

    return fcmp(result, decltype(result)(0)) ? 0 : result;
  }


  //|///////////////////// centroid /////////////////////////////////////////
  /// centroid of xy triangle
  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 2>* = nullptr>
  constexpr Point centroid(Point const &a, Point const &b, Point const &c)
  {
    return { (get<0>(a) + get<0>(b) + get<0>(c))/3, (get<1>(a) + get<1>(b) + get<1>(c))/3 };
  }


  //|///////////////////// quadrant /////////////////////////////////////////
  /// determines a quadrant index for the point around the origin
  namespace quadrant_impl
  {
    template<size_t i, size_t dimension, typename Point, std::enable_if_t<i == dimension>* = nullptr>
    int quadrant(Point const &)
    {
      return 0;
    }

    template<size_t i, size_t dimension, typename Point, std::enable_if_t<i != dimension>* = nullptr>
    int quadrant(Point const &pt)
    {
      return ((get<i>(pt) < 0) * (1 << (i))) + quadrant<i+1, dimension>(pt);
    }
  }

  template<typename Point, size_t dimension = dim<Point>()>
  int quadrant(Point const &pt)
  {
    return quadrant_impl::quadrant<0, dimension>(pt);
  }


  //|///////////////////// normal ///////////////////////////////////////////
  /// normal to line, triangle
  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 2>* = nullptr>
  constexpr auto normal(Point const &a, Point const &b)
  {
    return normalise(perp(vec(b, a)));
  }

  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr>
  constexpr auto normal(Point const &a, Point const &b, Point const &c)
  {
    return normalise(cross(vec(a, b), vec(a, c)));
  }


  //|///////////////////// slope ////////////////////////////////////////////
  /// slope (dy/dx) between two points
  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 2>* = nullptr>
  constexpr auto slope(Point const &a, Point const &b)
  {
    return (get<1>(b) - get<1>(a)) / (get<0>(b) - get<0>(a));
  }


  //|///////////////////// angle ////////////////////////////////////////////
  /// angle from one point to another
  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 2>* = nullptr>
  constexpr auto angle(Point const &a, Point const &b)
  {
    return std::atan2(get<1>(b) - get<1>(a), get<0>(b) - get<0>(a));
  }

  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr>
  constexpr auto anglex(Point const &a, Point const &b)
  {
    return std::atan2(get<2>(b) - get<2>(a), get<1>(b) - get<1>(a));
  }

  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr>
  constexpr auto angley(Point const &a, Point const &b)
  {
    return std::atan2(get<2>(b) - get<2>(a), get<0>(b) - get<0>(a));
  }

  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr>
  constexpr auto anglez(Point const &a, Point const &b)
  {
    return std::atan2(get<1>(b) - get<1>(a), get<0>(b) - get<0>(a));
  }


  //|///////////////////// rotate ///////////////////////////////////////////
  /// rotate point through angle radians
  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 2>* = nullptr>
  Point rotate(Point const &pt, coord_type_t<Point> yaw)
  {
    auto x = std::cos(yaw)*get<0>(pt) - std::sin(yaw)*get<1>(pt);
    auto y = std::sin(yaw)*get<0>(pt) + std::cos(yaw)*get<1>(pt);

    return { x, y };
  }

  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr>
  Point rotatex(Point const &pt, coord_type_t<Point> roll)
  {
    auto x = get<0>(pt);
    auto y = std::cos(roll)*get<1>(pt) - std::sin(roll)*get<2>(pt);
    auto z = std::sin(roll)*get<1>(pt) + std::cos(roll)*get<2>(pt);

    return { x, y, z };
  }

  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr>
  Point rotatey(Point const &pt, coord_type_t<Point> pitch)
  {
    auto x = std::cos(pitch)*get<0>(pt) + std::sin(pitch)*get<2>(pt);
    auto y = get<1>(pt);
    auto z = -std::sin(pitch)*get<0>(pt) + std::cos(pitch)*get<2>(pt);

    return { x, y, z };
  }

  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr>
  Point rotatez(Point const &pt, coord_type_t<Point> yaw)
  {
    auto x = std::cos(yaw)*get<0>(pt) - std::sin(yaw)*get<1>(pt);
    auto y = std::sin(yaw)*get<0>(pt) + std::cos(yaw)*get<1>(pt);
    auto z = get<2>(pt);

    return { x, y, z };
  }

  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr>
  Point rotate(Point const &pt, coord_type_t<Point> yaw, coord_type_t<Point> pitch, coord_type_t<Point> roll)
  {
    return rotatez(rotatey(rotatex(pt, roll), pitch), yaw);
  }


  //|///////////////////// nearest_on_line //////////////////////////////////
  /// nearest point on line
  template<typename Point>
  Point nearest_on_line(Point const &a, Point const &b, Point const &pt)
  {
    auto u = vec(a, b);

    return a + dot(vec(a, pt), u)/dot(u, u) * u;
  }


  //|///////////////////// nearest_on_segment ///////////////////////////////
  /// nearest point on segment
  template<typename Point>
  Point nearest_on_segment(Point const &a, Point const &b, Point const &pt)
  {
    auto u = vec(a, b);

    auto dot_ta = dot(vec(a, pt), u);

    if (dot_ta <= 0)
      return a;

    auto dot_tb = dot(vec(pt, b), u);

    if (dot_tb <= 0)
      return b;

    return a + dot_ta/(dot_ta + dot_tb) * u;
  }


  //|///////////////////// intersection /////////////////////////////////////
  /// intersection of two lines
  template<typename Point>
  struct lineintersect : public leap::optional<Point>
  {
    lineintersect() = default;
    lineintersect(Point const &point) : leap::optional<Point>(point) { }

    coord_type_t<Point> u;
    coord_type_t<Point> s;
    coord_type_t<Point> t;

    bool segseg() const { return u != 0 && s >= 0 && s <= 1 && t >= 0 && t <= 1; }
    bool segray() const { return u != 0 && s >= 0 && s <= 1 && t >= 0; }
    bool rayseg() const { return u != 0 && s >= 0 && t >= 0 && t <= 1; }
    bool rayray() const { return u != 0 && s >= 0 && t >= 0; }
    bool overlap() const { return region.has_value(); }

    leap::optional<std::pair<Point, Point>> region;
  };

  template<typename Point>
  auto intersection_robust(Point const &a1, Point const &a2, Point const &b1, Point const &b2)
  {
    lineintersect<Point> result;

    auto u = vec(a1, a2);
    auto v = vec(b1, b2);
    auto w = vec(b1, a1);

    auto sidea1 = orientation(a1, a2, b1);
    auto sidea2 = orientation(a1, a2, b2);
    auto sideb1 = orientation(b1, b2, a1);
    auto sideb2 = orientation(b1, b2, a2);

    if ((sidea1 == 0 && sidea2 == 0) || (sideb1 == 0 && sideb2 == 0))
    {
      result.u = 0;

      int k = 0;
      Point const *region[5];

      if (dot(v, v) != 0)
      {
        if (dot(vec(b1, a1), v) >= 0 && dot(vec(a1, b2), v) > 0)
          region[k++] = &a1;

        if (dot(vec(b1, a2), v) > 0 && dot(vec(a2, b2), v) >= 0)
          region[k++] = &a2;
      }

      if (dot(u, u) != 0)
      {
        if (dot(vec(a1, b1), u) > 0 && dot(vec(b1, a2), u) >= 0)
          region[k++] = &b1;

        if (dot(vec(a1, b2), u) >= 0 && dot(vec(b2, a2), u) > 0)
          region[k++] = &b2;
      }

      if (dot(u, u) == 0 && dot(v, v) == 0 && dot(w, w) == 0)
      {
        region[k++] = &a1;
        region[k++] = &b1;
      }

      if (k != 0)
      {
        result.region = std::make_pair(*region[0], *region[k-1]);
      }
    }
    else
    {
      result.u = perp(u, v);

      if (result.u != 0)
      {
        result.s = perp(v, w) / result.u;
        result.t = perp(u, w) / result.u;

        if ((sidea1 * sidea2) <= 0 && (sideb1 * sideb2) <= 0)
        {
          result.s = clamp(result.s, decltype(result.s)(0), decltype(result.s)(1));
          result.t = clamp(result.t, decltype(result.t)(0), decltype(result.t)(1));

          if (sideb1 == 0)
            result.s = 0;

          if (sideb2 == 0)
            result.s = 1;

          if (sidea1 == 0)
            result.t = 0;

          if (sidea2 == 0)
            result.t = 1;
        }

        result.emplace((std::abs(result.s) < std::abs(result.t)) ? a1 + result.s * u : b1 + result.t * v);
      }
    }

    return result;
  }

  template<typename Point>
  auto intersection(Point const &a1, Point const &a2, Point const &b1, Point const &b2)
  {
    lineintersect<Point> result;

    auto u = vec(a1, a2);
    auto v = vec(b1, b2);
    auto w = vec(b1, a1);

    result.u = perp(u, v);

    if (result.u != 0)
    {
      result.s = perp(v, w) / result.u;
      result.t = perp(u, w) / result.u;

      result.emplace((std::abs(result.s) < std::abs(result.t)) ? a1 + result.s * u : b1 + result.t * v);
    }

    return result;
  }


  //|///////////////////// intersection /////////////////////////////////////
  /// intersection of ray and triangle (moller–trumbore)
  template<typename Point, size_t dimension = dim<Point>(), std::enable_if_t<dimension == 3>* = nullptr>
  auto intersection(Point const &origin, Point const &direction, Point const &a, Point const &b, Point const &c)
  {
    leap::optional<Point> result;

    auto ab = vec(a, b);
    auto ac = vec(a, c);

    auto h = cross(direction, ac);
    auto d = dot(ab, h);

    if (d != 0)
    {
      auto f = 1 / d;
      auto s = vec(a, origin);
      auto u = dot(s, h) * f;

      if (u >= 0 && u <= 1)
      {
        auto q = cross(s, ab);
        auto v = dot(direction, q) * f;

        if (v >= 0 && u + v <= 1)
        {
          auto t = dot(ac, q) * f;

          if (t >= 0)
          {
            result.emplace(origin + t * direction);
          }
        }
      }
    }

    return result;
  }


  //|///////////////////// nearest_on_polyline //////////////////////////////
  /// nearest point on line
  template<typename InputIterator>
  auto nearest_on_polyline(InputIterator f, InputIterator l, typename std::iterator_traits<InputIterator>::value_type const &pt)
  {
    auto result = typename std::iterator_traits<InputIterator>::value_type();

    auto mindist = std::numeric_limits<decltype(distsqr(result, result))>::max();

    for(auto ic = std::next(f), ip = f; ic != l; ip = ic, ++ic)
    {
      auto np = nearest_on_segment(*ip, *ic, pt);

      auto dist = distsqr(np, pt);

      if (dist < mindist)
      {
        result = np;
        mindist = dist;
      }
    }

    return result;
  }

  template<typename Polyline, typename Point>
  auto nearest_on_polyline(Polyline const &polyline, Point const &pt)
  {
    return nearest_on_polyline(std::begin(polyline), std::end(polyline), pt);
  }


  //|///////////////////// simplify /////////////////////////////////////////
  /// simplify polyline (ramer–douglas–peucker)
  template<typename InputIterator>
  InputIterator simplify(InputIterator f, InputIterator l, double epsilon)
  {
    using std::swap;

    if (f == l || std::next(f) == l || std::next(std::next(f)) == l)
      return l;

    auto a = f;
    auto b = std::prev(l);
    auto c = l;

    auto maxdist = decltype(distsqr(*a, *c))(0);

    for(auto i = std::next(f); i != std::prev(l); ++i)
    {
      auto dist = distsqr(nearest_on_segment(*a, *b, *i), *i);

      if (dist > maxdist)
      {
        c = i;
        maxdist = dist;
      }
    }

    if (maxdist > epsilon)
    {
      auto j = simplify(f, std::next(c), epsilon);
      auto k = simplify(c, l, epsilon);

      if (j == std::next(c))
        return k;

      for(auto i = std::next(c); i != k; ++i)
        swap(*j++, *i);

      return j;
    }
    else
    {
      swap(*std::next(f), *std::prev(l));

      return std::next(std::next(f));
    }
  }

  template<typename Polyline>
  void simplify(Polyline &polyline, double epsilon)
  {
    polyline.erase(simplify(std::begin(polyline), std::end(polyline), epsilon), std::end(polyline));
  }


  /**
  *  @}
  **/

} // namespace lml
} // namespace leap
