//
// Datum - ibl
//

#include "ibl.h"
#include "datum/math.h"
#include "assetpacker.h"
#include <iostream>

using namespace std;
using namespace lml;
using namespace leap;

namespace
{
  struct Sampler
  {
    Sampler(int width, int height, uint32_t *bits)
      : width(width), height(height)
    {
      faces[0] = bits + 0 * width * height; // right
      faces[1] = bits + 1 * width * height; // left
      faces[2] = bits + 2 * width * height; // down
      faces[3] = bits + 3 * width * height; // up
      faces[4] = bits + 4 * width * height; // forward
      faces[5] = bits + 5 * width * height; // back
    }

    Color4 sample(int face, int i, int j) const
    {
      return rgbe(faces[face][j * width + i]);
    }

    Color4 sample(int face, Vec2 const &texcoords) const
    {
      float i, j;
      auto u = modf(fmod2(texcoords.x, 1.0f) * (width - 1), &i);
      auto v = modf(fmod2(texcoords.y, 1.0f) * (height - 1), &j);

      return lerp(lerp(sample(face, (int)i, (int)j), sample(face, (int)i+1, (int)j), u), lerp(sample(face, (int)i, (int)j+1), sample(face, (int)i+1, (int)j+1), u), v);
    }

    Color4 sample(Vec3 const &texcoords) const
    {
      Color4 result;

      auto mx = abs(texcoords.x);
      auto my = abs(texcoords.y);
      auto mz = abs(texcoords.z);

      if (mx > max(my, mz))
      {
        if (texcoords.x > 0)
        {
          result = sample(0, Vec2(0.5f + 0.5f*texcoords.z/mx, 0.5f + 0.5f*texcoords.y/mx));
        }
        else
        {
          result = sample(1, Vec2(0.5f - 0.5f*texcoords.z/mx, 0.5f + 0.5f*texcoords.y/mx));
        }
      }

      else if (my > max(mx, mz))
      {
        if (texcoords.y > 0)
        {
          result = sample(3, Vec2(0.5f + 0.5f*texcoords.x/my, 0.5f + 0.5f*texcoords.z/my));
        }
        else
        {
          result = sample(2, Vec2(0.5f + 0.5f*texcoords.x/my, 0.5f - 0.5f*texcoords.z/my));
        }
      }

      else if (mz > max(mx, my))
      {
        if (texcoords.z > 0)
        {
          result = sample(5, Vec2(0.5f - 0.5f*texcoords.x/mz, 0.5f + 0.5f*texcoords.y/mz));
        }
        else
        {
          result = sample(4, Vec2(0.5f + 0.5f*texcoords.x/mz, 0.5f + 0.5f*texcoords.y/mz));
        }
      }

      return result;
    }

    int width;
    int height;
    uint32_t *faces[6];
  };

  float radicalinverse_VdC(uint32_t bits)
  {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
  }

  Vec2 hammersley(int i, int samples)
  {
    return Vec2(float(i)/float(samples), radicalinverse_VdC(i));
  }

  float GGX(float NdotV, float alpha)
  {
    float k = alpha / 2;
    return NdotV / (NdotV * (1.0f - k) + k);
  }

  Vec3 importancesample_ggx(Vec2 u, float alpha, Vec3 normal)
  {
    float phi = 2*pi<float>() * u.x;
    float costheta = sqrt((1 - u.y) / (1 + (alpha*alpha - 1) * u.y));
    float sintheta = sqrt(1 - costheta*costheta);

    Vec3 up = abs(normal.z) < 0.999 ? Vec3(0,0,1) : Vec3(1,0,0);
    Vec3 tangent = normalise(cross(up, normal));
    Vec3 bitangent = cross(normal, tangent);

    return sintheta * cos(phi) * tangent + sintheta * sin(phi) * bitangent + costheta * normal;
  }

  Color3 convolve(float roughness, Vec3 ray, Sampler const &envmap)
  {
    constexpr int kSamples = 1024;

    Vec3 N = ray;
    Vec3 V = ray;

    Color3 sum(0, 0, 0);
    float totalweight = 0;

    for(int i = 0; i < kSamples; ++i)
    {
      Vec2 u = hammersley(i, kSamples);
      Vec3 H = importancesample_ggx(u, roughness * roughness, N);
      Vec3 L = 2 * dot(V, H) * H - V;

      float NdotL = clamp(dot(N, L), 0.0f, 1.0f);

      if (NdotL > 0)
      {
        sum += envmap.sample(L).rgb * NdotL;

        totalweight += NdotL;
      }
    }

    return sum / totalweight;
  }

  Vec2 integrate(float roughness, float NdotV)
  {
    constexpr int kSamples = 1024;

    Vec3 V = Vec3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);

    float a = 0;
    float b = 0;

    for(int i = 0; i < kSamples; ++i)
    {
      Vec2 u = hammersley(i, kSamples);
      Vec3 H = importancesample_ggx(u, roughness * roughness, Vec3(0, 0, 1));
      Vec3 L = 2 * dot(V, H) * H - V;

      float NdotL = clamp(L.z, 0.0f, 1.0f);
      float NdotH = clamp(H.z, 0.0f, 1.0f);
      float VdotH = clamp(dot(V, H), 0.0f, 1.0f);

      if (NdotL > 0)
      {
        float G = GGX(NdotL, roughness * roughness) * GGX(NdotV, roughness * roughness);
        float Vis = G * VdotH / (NdotH * NdotV);
        float Fc = pow(1 - VdotH, 5.0f);

        a += (1 - Fc) * Vis;
        b += Fc * Vis;
      }
    }

    return Vec2(a, b) / kSamples;
  }
}


///////////////////////// image_pack_cubemap_ibl ////////////////////////////
void image_buildmips_cube_ibl(int width, int height, int levels, void *bits)
{
  uint32_t *src = (uint32_t*)bits;
  uint32_t *dst = src + width * height * 6;

  for(int level = 1; level < levels; ++level)
  {
    Sampler envmap(width, height, src);

    float roughness = (float)level / (float)(levels - 1);

    Transform transforms[] =
    {
      Transform::rotation(Vec3(0, 1, 0), -pi<float>()/2), // right
      Transform::rotation(Vec3(0, 1, 0), pi<float>()/2),  // left
      Transform::rotation(Vec3(1, 0, 0), -pi<float>()/2), // bottom
      Transform::rotation(Vec3(1, 0, 0), pi<float>()/2),  // top
      Transform::rotation(Vec3(0, 1, 0), 0),              // front
      Transform::rotation(Vec3(0, 1, 0), pi<float>()),    // back
    };

    for(auto &transform : transforms)
    {
      for(int y = 0, end = height >> 1; y < end; ++y)
      {
        for(int x = 0, end = width >> 1; x < end; ++x)
        {
          *dst++ = rgbe(convolve(roughness, transform * normalise(Vec3(2 * (x + 0.5f)/(width >> 1) - 1, 2 * (y + 0.5f)/(height >> 1) - 1, -1.0)), envmap));
        }
      }
    }

    src += width * height * 6;

    width /= 2;
    height /= 2;
  }
}


///////////////////////// image_pack_cube_ibl ///////////////////////////////
void image_pack_cube_ibl(HDRImage const &image, int width, int height, int levels, void *bits)
{
  image_pack_cube(image, width, height, 1, bits);

  image_buildmips_cube_ibl(width, height, levels, bits);
}


///////////////////////// image_pack_envbrdf ////////////////////////////////
void image_pack_envbrdf(int width, int height, void *bits)
{
  uint32_t *dst = (uint32_t*)bits;

  for(int y = 0; y < height; ++y)
  {
    for(int x = 0; x < width; ++x)
    {
      float NdotV = (x + 0.5f) / width;
      float roughness = (y + 0.5f) / height;

      auto lut = integrate(roughness, NdotV);

      *dst++ = rgbe(Color4(lut.x, lut.y, 0.0f, 1.0f));
    }
  }
}


///////////////////////// image_pack_watercolor /////////////////////////////
void image_pack_watercolor(Color3 const &deepcolor, Color3 const &shallowcolor, float depthscale, Color3 const &fresnelcolor, float fresnelbias, float fresnelpower, int width, int height, void *bits)
{
  uint32_t *dst = (uint32_t*)bits;

  for(int y = 0; y < height; ++y)
  {
    for(int x = 0; x < width; ++x)
    {
      float scale = (x + 0.5f) / width;
      float facing = (y + 0.5f) / height;
      float fresnel = clamp(fresnelbias + pow(facing, fresnelpower), 0.0f, 1.0f);

      auto color = lerp(shallowcolor, deepcolor, clamp(1 - exp2(-depthscale * scale * 100.0f), 0.0f, 1.0f));

      *dst++ = rgbe(lerp(color, fresnelcolor, fresnel));
    }
  }
}
