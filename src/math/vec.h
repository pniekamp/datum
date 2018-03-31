//
// Datum - vector types
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include <leap/lml/vector.h>
#include <leap/lml/geometry.h>
#include <leap/lml/io.h>

namespace lml
{
  using namespace leap::lml;

  //|-------------------- Vec2 ----------------------------------------------
  //|------------------------------------------------------------------------

  class Vec2 : public VectorView<Vec2, float, 0, 1>
  {
    public:
      Vec2() = default;
      explicit constexpr Vec2(float k);
      constexpr Vec2(float x, float y);

    union
    {
      struct
      {
        float x;
        float y;
      };
    };
  };


  //|///////////////////// Vec2::Constructor ////////////////////////////////
  constexpr Vec2::Vec2(float k)
    : x(k), y(k)
  {
  }


  //|///////////////////// Vec2::Constructor ////////////////////////////////
  constexpr Vec2::Vec2(float x, float y)
    : x(x), y(y)
  {
  }


  //|///////////////////// Matrix Multiply //////////////////////////////////
  constexpr Vec2 operator *(Matrix2f const &m, Vec2 const &v)
  {
    auto x = m(0,0)*v.x + m(0,1)*v.y;
    auto y = m(1,0)*v.x + m(1,1)*v.y;

    return { x, y };
  }


  //|-------------------- Vec3 ----------------------------------------------
  //|------------------------------------------------------------------------

  class Vec3 : public VectorView<Vec3, float, 0, 1, 2>
  {
    public:
      Vec3() = default;
      explicit constexpr Vec3(float k);
      constexpr Vec3(float x, float y, float z);
      constexpr Vec3(Vec2 const &xy, float z);

    union
    {
      struct
      {
        float x;
        float y;
        float z;
      };

      Vec2 xy;
    };
  };


  //|///////////////////// Vec3::Constructor ////////////////////////////////
  constexpr Vec3::Vec3(float k)
    : x(k), y(k), z(k)
  {
  }


  //|///////////////////// Vec3::Constructor ////////////////////////////////
  constexpr Vec3::Vec3(float x, float y, float z)
    : x(x), y(y), z(z)
  {
  }


  //|///////////////////// Vec3::Constructor ////////////////////////////////
  constexpr Vec3::Vec3(Vec2 const &xy, float z)
    : Vec3(xy.x, xy.y, z)
  {
  }


  //|///////////////////// Matrix Multiply //////////////////////////////////
  constexpr Vec3 operator *(Matrix3f const &m, Vec3 const &v)
  {
    auto x = m(0,0)*v.x + m(0,1)*v.y + m(0,2)*v.z;
    auto y = m(1,0)*v.x + m(1,1)*v.y + m(1,2)*v.z;
    auto z = m(2,0)*v.x + m(2,1)*v.y + m(2,2)*v.z;

    return { x, y, z };
  }


  //|-------------------- Vec4 ----------------------------------------------
  //|------------------------------------------------------------------------

  class Vec4 : public VectorView<Vec4, float, 0, 1, 2, 3>
  {
    public:
      Vec4() = default;
      explicit constexpr Vec4(float k);
      constexpr Vec4(float x, float y, float z, float w);
      constexpr Vec4(Vec2 const &xy, float z, float w);
      constexpr Vec4(Vec3 const &xyz, float w);

    union
    {
      struct
      {
        float x;
        float y;
        float z;
        float w;
      };

      Vec2 xy;

      Vec3 xyz;
    };
  };


  //|///////////////////// Vec4::Constructor ////////////////////////////////
  constexpr Vec4::Vec4(float k)
    : x(k), y(k), z(k), w(k)
  {
  }


  //|///////////////////// Vec4::Constructor ////////////////////////////////
  constexpr Vec4::Vec4(float x, float y, float z, float w)
    : x(x), y(y), z(z), w(w)
  {
  }


  //|///////////////////// Vec4::Constructor ////////////////////////////////
  constexpr Vec4::Vec4(Vec2 const &xy, float z, float w)
    : Vec4(xy.x, xy.y, z, w)
  {
  }


  //|///////////////////// Vec4::Constructor ////////////////////////////////
  constexpr Vec4::Vec4(Vec3 const &xyz, float w)
    : Vec4(xyz.x, xyz.y, xyz.z, w)
  {
  }


  //|///////////////////// Matrix Multiply //////////////////////////////////
  constexpr Vec4 operator *(Matrix4f const &m, Vec4 const &v)
  {
    auto x = m(0,0)*v.x + m(0,1)*v.y + m(0,2)*v.z + m(0,3)*v.w;
    auto y = m(1,0)*v.x + m(1,1)*v.y + m(1,2)*v.z + m(1,3)*v.w;
    auto z = m(2,0)*v.x + m(2,1)*v.y + m(2,2)*v.z + m(2,3)*v.w;
    auto w = m(3,0)*v.x + m(3,1)*v.y + m(3,2)*v.z + m(3,3)*v.w;

    return { x, y, z, w };
  }
}
