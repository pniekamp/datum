//
// Datum - color
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include <leap/lml/vector.h>

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
      constexpr Color4(float r, float g, float b, float a = 1.0f);
      constexpr Color4(Color3 const &rgb, float a = 1.0f);

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



  //|///////////////////// Gamma ////////////////////////////////////////////
  inline Color4 gamma(Color4 const &color)
  {
    return Color4(pow(color.r, 1/2.2f), pow(color.g, 1/2.2f), pow(color.b, 1/2.2f), color.a);
  }

  inline Color4 ungamma(Color4 const &color)
  {
    return Color4(pow(color.r, 2.2f), pow(color.g, 2.2f), pow(color.b, 2.2f), color.a);
  }


  //|///////////////////// RGBA /////////////////////////////////////////////
  inline uint32_t rgba(Color4 const &color)
  {
    return ((uint8_t)(color.b * 255) << 0) | ((uint8_t)(color.g * 255) << 8) | ((uint8_t)(color.r * 255) << 16) | ((uint8_t)(color.a * 255) << 24);
  }

  inline Color4 rgba(uint32_t color)
  {
    return Color4((uint8_t)(color >> 16) / 255.0f, (uint8_t)(color >> 8) / 255.0f, (uint8_t)(color >> 0) / 255.0f, (uint8_t)(color >> 24) / 255.0f);
  }

  inline uint32_t srgba(Color4 const &color)
  {
    return rgba(gamma(color));
  }

  inline Color4 srgba(uint32_t color)
  {
    return ungamma(rgba(color));
  }


  //|///////////////////// RGBM /////////////////////////////////////////////
  inline uint32_t rgbm(Color4 const &color)
  {
    auto r = color.r * 1.0f / 8.0f;
    auto g = color.g * 1.0f / 8.0f;
    auto b = color.b * 1.0f / 8.0f;
    auto m = std::ceil(clamp(std::max(r, std::max(g, std::max(b, 1e-6f))), 0.0f, 1.0f) * 255.0f) / 255.0f;

    return ((uint8_t)(b/m * 255) << 0) | ((uint8_t)(g/m * 255) << 8) | ((uint8_t)(r/m * 255) << 16) | ((uint8_t)(m * 255) << 24);
  }

  inline Color4 rgbm(uint32_t color)
  {
    auto r = (uint8_t)(color >> 16) / 255.0f;
    auto g = (uint8_t)(color >> 8) / 255.0f;
    auto b = (uint8_t)(color >> 0) / 255.0f;
    auto m = (uint8_t)(color >> 24) / 255.0f;

    return Color4(8.0f * r * m, 8.0f * g * m, 8.0f * b * m, 1.0f);
  }


  //|///////////////////// RGBE /////////////////////////////////////////////
  inline uint32_t rgbe(Color4 const &color)
  {
    auto r = clamp(color.r, 0.0f, 65408.0f);
    auto g = clamp(color.g, 0.0f, 65408.0f);
    auto b = clamp(color.b, 0.0f, 65408.0f);
    auto e = std::max(-16.0f, std::floor(std::log2(std::max(r, std::max(g, b))))) + 1;

    return ((uint8_t)(e + 15) << 27) | ((uint16_t)(r / std::exp2(e) * 511) << 0) | ((uint16_t)(g / std::exp2(e) * 511) << 9) | ((uint16_t)(b / std::exp2(e) * 511) << 18);
  }

  inline Color4 rgbe(uint32_t color)
  {
    auto r = ((color >> 0) & 0x1FF) / 511.0f;
    auto g = ((color >> 9) & 0x1FF) / 511.0f;
    auto b = ((color >> 18) & 0x1FF) / 511.0f;
    auto e = (int)((color >> 27) & 0x1F) - 15;

    return Color4(r * std::exp2(e), g * std::exp2(e), b * std::exp2(e), 1.0f);
  }


  //|///////////////////// HSV //////////////////////////////////////////////
  inline Color3 hsv(float h, float s, float v)
  {
    if (v <= 0)
      return Color3(0, 0, 0);

    if (s <= 0)
      return Color3(v, v, v);

    auto hf = clamp(h, 0.0f, 360.0f) / 60.0f;

    auto i = (int)hf;

    auto p = v * (1 - s);
    auto q = v * (1 - (s * (hf - i)));
    auto t = v * (1 - (s * (1 - (hf - i))));

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


  //|///////////////////// premultiply //////////////////////////////////////
  inline Color4 premultiply(Color4 const &color)
  {
    return Color4(color.r * color.a, color.g * color.a, color.b * color.a, color.a);
  }

} // namespace

