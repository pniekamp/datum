//
// Datum - color
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "leap/lml/vector.h"

namespace lml
{
  using namespace leap::lml;


  //|-------------------- Color3 --------------------------------------------
  //|------------------------------------------------------------------------

  class Color3 : public VectorView<Color3, float, 0, 1, 2>
  {
    public:
      Color3() = default;
      constexpr Color3(float r, float g, float b);

    union
    {
      struct
      {
        float r;
        float g;
        float b;
      };
    };
  };


  //|///////////////////// Color3::Constructor //////////////////////////////
  constexpr Color3::Color3(float r, float g, float b)
    : r(r), g(g), b(b)
  {
  }




  //|-------------------- Color4 --------------------------------------------
  //|------------------------------------------------------------------------

  class Color4 : public VectorView<Color4, float, 0, 1, 2, 3>
  {
    public:
      Color4() = default;
      constexpr Color4(float r, float g, float b, float a = 1.0);
      constexpr Color4(Color3 const &rgb, float a = 1.0);

    union
    {
      struct
      {
        float r;
        float g;
        float b;
        float a;
      };

      Color3 rgb;
    };
  };


  //|///////////////////// Color4::Constructor //////////////////////////////
  constexpr Color4::Color4(float r, float g, float b, float a)
    : r(r), g(g), b(b), a(a)
  {
  }

  //|///////////////////// Color4::Constructor //////////////////////////////
  constexpr Color4::Color4(Color3 const &rgb, float a)
    : r(rgb.r), g(rgb.g), b(rgb.b), a(a)
  {
  }



  //|///////////////////// Color RGB ////////////////////////////////////////
  inline uint32_t rgba(Color4 const &color)
  {
    return ((uint8_t)(color.b * 255) << 0) + ((uint8_t)(color.g * 255) << 8) + ((uint8_t)(color.r * 255) << 16) + ((uint8_t)(color.a * 255) << 24);
  }


  //|///////////////////// Color HSV ////////////////////////////////////////
  inline Color3 hsv(float h, float s, float v)
  {
    if (v <= 0)
      return Color3(0, 0, 0);

    if (s <= 0)
      return Color3(v, v, v);

    float hf = clamp(h, 0.0f, 360.0f) / 60.0f;

    int i = (int)hf;

    float p = v * (1 - s);
    float q = v * (1 - (s * (hf - i)));
    float t = v * (1 - (s * (1.0 - (hf - i))));

    switch(i)
    {
      case 0:
        return Color3(v, t, p);

      case 1:
        return Color3(q, v, p);

      case 2:
        return Color3(p, v, t);

      case 3:
        return Color3(p, q, v);

      case 4:
        return Color3(t, p, v);

      case 5:
        return Color3(v, p, q);

      case 6:
        return Color3(v, t, p);

      case -1:
        return Color3(v, p, q);
    }

    return Color3(0, 0, 0);
  }
}
