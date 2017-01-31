//
// Datum - line
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "vec.h"
#include "transform.h"

namespace lml
{

  //|---------------------- Line --------------------------------------------
  //|------------------------------------------------------------------------

  class Line
  {
    public:
      Line() = default;
      constexpr Line(Vec3 const &a, Vec3 const &b);

      Vec3 a;
      Vec3 b;
  };


  ///////////////////////// Line::Constructor ///////////////////////////////
  constexpr Line::Line(Vec3 const &a, Vec3 const &b)
    : a(a),
      b(b)
  {
  }


  //////////////////////// Line operator == /////////////////////////////////
  inline bool operator ==(Line const &lhs, Line const &rhs)
  {
    return (lhs.a == rhs.a) && (lhs.b == rhs.b);
  }


  //////////////////////// Line operator != /////////////////////////////////
  inline bool operator !=(Line const &lhs, Line const &rhs)
  {
    return !(lhs == rhs);
  }


  //////////////////////// Line stream << /////////////////////////////////
  inline std::ostream &operator <<(std::ostream &os, Line const &line)
  {
    os << "[Line:" << line.a << "," << line.b << "]";

    return os;
  }


  //////////////////////// distsqr //////////////////////////////////////////
  /// distance between line and point squared
  inline float distsqr(Line const &line, Vec3 const &pt)
  {
    return normsqr(cross(pt - line.a, pt - line.b)) / normsqr(line.b - line.a);
  }


  //////////////////////// dist /////////////////////////////////////////////
  /// distance between line and point
  inline float dist(Line const &line, Vec3 const &pt)
  {
    return std::sqrt(distsqr(line, pt));
  }


  //////////////////////// nearest_on_line //////////////////////////////////
  /// nearest point on line
  inline Vec3 nearest_on_line(Line const &line, Vec3 const &pt)
  {
    return nearest_on_line(line.a, line.b, pt);
  }


  //////////////////////// nearest_on_segment ///////////////////////////////
  /// nearest point on line segment
  inline Vec3 nearest_on_segment(Line const &line, Vec3 const &pt)
  {
    return nearest_on_segment(line.a, line.b, pt);
  }


  //////////////////////// transform ////////////////////////////////////////
  /// transform line
  inline Line operator *(Transform const &transform, Line const &line)
  {
    return Line(transform * line.a, transform * line.b);
  }
}
