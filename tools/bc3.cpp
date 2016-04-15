//
// Datum - bc3 compression
//

#include "bc3.h"
#include "datum/math.h"
#include <iostream>

using namespace std;
using namespace lml;


//|///////////////////// rgb565 /////////////////////////////////////////////

inline Color4 rgb565(uint16_t color)
{
   return Color4(((color >> 11) & 31) / 31.0f, ((color >>  5) & 63) / 63.0f, ((color >>  0) & 31) / 31.0f, 1.0f);
}

inline uint16_t rgb565(Color4 const &color)
{
  return (((uint8_t)(color.r * 31.0f + 0.5f) << 11) | ((uint8_t)(color.g * 63.0f + 0.5f) << 5) | (uint8_t)(color.b * 31.0f + 0.5f) << 0);
}


//|---------------------- BC3 -----------------------------------------------
//|--------------------------------------------------------------------------

//|///////////////////// optimise_color /////////////////////////////////////
void optimise_color(Color3 *color0, Color3 *color1, Color4 color[16])
{
  static const float epsilon = (0.25f / 64.0f) * (0.25f / 64.0f);

  Color3 mincolor = *color0;
  Color3 maxcolor = *color1;

  if (normsqr(maxcolor - mincolor) > 1e-6f)
  {
    auto midcolor = (mincolor + maxcolor) / 2;

    auto diagonal = normalise(maxcolor - mincolor);

    float direction[4] = {};

    for(size_t i = 0; i < 16; ++i)
    {
      auto pt = hada(color[i].rgb - midcolor, diagonal);

      direction[0] += pow(pt.r + pt.g + pt.b, 2);
      direction[1] += pow(pt.r + pt.g - pt.b, 2);
      direction[2] += pow(pt.r - pt.g + pt.b, 2);
      direction[3] += pow(pt.r - pt.g - pt.b, 2);
    }

    auto maxdirection = max_element(begin(direction), end(direction));

    if ((maxdirection - begin(direction)) & 2)
    {
      swap(mincolor.g, maxcolor.g);
    }

    if ((maxdirection - begin(direction)) & 1)
    {
      swap(mincolor.b, maxcolor.b);
    }
  }

  for(size_t iteration = 0; iteration < 8; ++iteration)
  {
    Color3 palette[4];
    palette[0] = mincolor;
    palette[1] = lerp(mincolor, maxcolor, 1.0f/3.0f);
    palette[2] = lerp(mincolor, maxcolor, 2.0f/3.0f);
    palette[3] = maxcolor;

    if (normsqr(maxcolor - mincolor) < 1.0f / 4096.0f)
      break;

    Color3 dx = {}, dy = {};
    float d2x = 0, d2y = 0;

    Color3 scale = 3.0f * normalise(maxcolor - mincolor) / norm(maxcolor - mincolor);

    for(size_t i = 0; i < 16; ++i)
    {
      size_t index = clamp((int)(dot(color[i].rgb - mincolor, scale) + 0.5f), 0, 3);

      dx += (1 - index/3.0f) / 8.0f * (palette[index] - color[i].rgb);
      dy += (index/3.0f) / 8.0f * (palette[index] - color[i].rgb);

      d2x += pow(1 - index/3.0f, 2) / 8.0f;
      d2y += pow(index/3.0f, 2) / 8.0f;
    }

    if (d2x > 0)
    {
      mincolor = clamp(mincolor - dx / d2x, 0.0f, 1.0f);
    }

    if (d2y > 0)
    {
      maxcolor = clamp(maxcolor - dy / d2y, 0.0f, 1.0f);
    }

    if (rgba(maxcolor) < rgba(mincolor))
    {
      swap(mincolor, maxcolor);
    }

    if (normsqr(dx) < epsilon && normsqr(dy) < epsilon)
      break;
  }

  *color0 = rgb565(rgb565(mincolor)).rgb;
  *color1 = rgb565(rgb565(maxcolor)).rgb;
}


//|///////////////////// encode_color ///////////////////////////////////////
void encode_color(uint32_t block[16], BC3 *result)
{
  Color4 color[16];
  for(size_t i = 0; i < 16; ++i)
  {
    color[i] = rgb565(rgb565(rgba(block[i])));
  }

  Color3 color0 = color[0].rgb;
  Color3 color1 = color[0].rgb;

  for(size_t i = 1; i < 16; ++i)
  {
    color0 = lml::min(color0, color[i].rgb);
    color1 = lml::max(color1, color[i].rgb);
  }

  optimise_color(&color0, &color1, color);

  result->color0 = rgb565(color0);
  result->color1 = rgb565(color1);
  result->indices = 0x00000000;

  if (result->color0 != result->color1)
  {
    static const size_t palette[] = { 0, 2, 3, 1 };

    Color3 scale = 3.0f * normalise(color1 - color0) / norm(color1 - color0);

    for(size_t i = 0; i < 16; ++i)
    {
      size_t index = clamp((int)(dot(color[i].rgb - color0, scale) + 0.5f), 0, 3);

      result->indices = (palette[index] << 30) | (result->indices >> 2);
    }
  }
}


//|///////////////////// optimise_alpha /////////////////////////////////////
void optimise_alpha(float *alpha0, float *alpha1, float alpha[16])
{
  static const float epsilon = (1.0f / 64.0f) * (1.0f / 64.0f);

  float minalpha = *alpha0;
  float maxalpha = *alpha1;

  for(size_t iteration = 0; iteration < 8; ++iteration)
  {
    float palette[8];
    palette[0] = minalpha;
    palette[1] = lerp(minalpha, maxalpha, 1.0f / 7.0f);
    palette[2] = lerp(minalpha, maxalpha, 2.0f / 7.0f);
    palette[3] = lerp(minalpha, maxalpha, 3.0f / 7.0f);
    palette[4] = lerp(minalpha, maxalpha, 4.0f / 7.0f);
    palette[5] = lerp(minalpha, maxalpha, 5.0f / 7.0f);
    palette[6] = lerp(minalpha, maxalpha, 6.0f / 7.0f);
    palette[7] = maxalpha;

    if (maxalpha - minalpha < 1.0f / 256.0f)
      break;

    float dx = 0, dy = 0;
    float d2x = 0, d2y = 0;

    float scale = 7.0f / (maxalpha - minalpha);

    for(int i = 0; i < 16; ++i)
    {
      size_t index = clamp((int)((alpha[i] - minalpha) * scale + 0.5f), 0, 7);

      dx += (1 - index/7.0f) * (palette[index] - alpha[i]);
      dy += (index/7.0f) * (palette[index] - alpha[i]);

      d2x += pow(1 - index/7.0f, 2);
      d2y += pow(index/7.0f, 2);
    }

    if (d2x > 0.0f)
    {
      minalpha = clamp(minalpha - dx / d2x, 0.0f, 1.0f);
    }

    if (d2y > 0.0f)
    {
      maxalpha = clamp(maxalpha - dy / d2y, 0.0f, 1.0f);
    }

    if (maxalpha < minalpha)
    {
      swap(minalpha, maxalpha);
    }

    if (dx*dx < epsilon && dy*dy < epsilon)
      break;
  }

  *alpha0 = minalpha;
  *alpha1 = maxalpha;
}


//|///////////////////// encode_alpha ///////////////////////////////////////
void encode_alpha(uint32_t block[16], BC3 *result)
{
  float alpha[16];
  for(size_t i = 0; i < 16; ++i)
  {
    alpha[i] = rgba(block[i]).a;
  }

  float alpha0 = alpha[0];
  float alpha1 = alpha[0];

  for(size_t i = 1; i < 16; ++i)
  {
    alpha0 = min(alpha0, alpha[i]);
    alpha1 = max(alpha1, alpha[i]);
  }

  optimise_alpha(&alpha0, &alpha1, alpha);

  result->alpha0 = alpha1 * 255.0 + 0.5f;
  result->alpha1 = alpha0 * 255.0 + 0.5f;
  memset(result->bitmap, 0x00, 6);

  if (alpha0 != alpha1)
  {
    static const size_t palette[] = { 1, 7, 6, 5, 4, 3, 2, 0 };

    float scale = 7.0f / (alpha1 - alpha0);

    for(size_t k = 0; k < 2; ++k)
    {
      uint32_t bitmap = 0;

      for(size_t i = k * 8; i < k * 8 + 8; ++i)
      {
        size_t index = clamp((int)((alpha[i] - alpha0) * scale + 0.5f), 0, 7);

        bitmap = (palette[index] << 29) | (bitmap >> 3);
      }

      result->bitmap[3*k + 0] = bitmap >> 8;
      result->bitmap[3*k + 1] = bitmap >> 16;
      result->bitmap[3*k + 2] = bitmap >> 24;
    }
  }
}


//|///////////////////// encode /////////////////////////////////////////////
void encode(uint32_t block[16], BC3 *result)
{
  encode_color(block, result);
  encode_alpha(block, result);
}
