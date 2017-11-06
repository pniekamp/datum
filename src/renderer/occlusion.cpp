//
// Datum - occlusion
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "occlusion.h"
#include "debug.h"

using namespace std;
using namespace lml;

namespace
{
  enum ClipMask
  {
    ClipPosX = 0x01,
    ClipNegX = 0x02,
    ClipPosY = 0x04,
    ClipNegY = 0x08,
    ClipPosZ = 0x10,
    ClipNegZ = 0x20
  };

  struct Gradients
  {
    Gradients(Vec3 *vertices)
    {
      float invdx = 1.0f / ((vertices[1].x - vertices[2].x) * (vertices[0].y - vertices[2].y) - (vertices[0].x - vertices[2].x) * (vertices[1].y - vertices[2].y));

      dzdx = invdx * ((vertices[1].z - vertices[2].z) * (vertices[0].y - vertices[2].y) - (vertices[0].z - vertices[2].z) * (vertices[1].y - vertices[2].y));
      dzdy = -invdx * ((vertices[1].z - vertices[2].z) * (vertices[0].x - vertices[2].x) - (vertices[0].z - vertices[2].z) * (vertices[1].x - vertices[2].x));
    }

    float dzdx;
    float dzdy;
  };


  struct Edge
  {
    Edge(Gradients const &gradients, Vec3 top, Vec3 bot)
    {
      x = top.x;
      xstep = (bot.x - top.x) / (bot.y - top.y);
      z = top.z;
      zstep = xstep * gradients.dzdx + gradients.dzdy;
    }

    float x;
    float xstep;
    float z;
    float zstep;
  };

  ///////////////////////// rasterize ///////////////////////////////////////
  void rasterize(float (&buffer)[OcclusionBuffer::Height][OcclusionBuffer::Width], Vec4 *vertices)
  {
    Vec3 positions[3];

    for(int i = 0; i < 3; ++i)
    {
      float invw = 1.0f / vertices[i].w;

      positions[i].x = (0.5f * vertices[i].x * invw + 0.5f) * (OcclusionBuffer::Width - 1);
      positions[i].y = (0.5f * vertices[i].y * invw + 0.5f) * (OcclusionBuffer::Height - 1);
      positions[i].z = (vertices[i].z * invw);
    }

    int top, mid, bot, lefttoright;

    if (positions[0].y < positions[1].y)
    {
      if (positions[2].y < positions[0].y)
      {
        top = 2;
        mid = 0;
        bot = 1;
        lefttoright = 1;
      }
      else if (positions[1].y < positions[2].y)
      {
        top = 0;
        mid = 1;
        bot = 2;
        lefttoright = 1;
      }
      else
      {
        top = 0;
        mid = 2;
        bot = 1;
        lefttoright = 0;
      }
    }
    else
    {
      if (positions[2].y < positions[1].y)
      {
        top = 2;
        mid = 1;
        bot = 0;
        lefttoright = 0;
      }
      else if (positions[0].y < positions[2].y)
      {
        top = 1;
        mid = 0;
        bot = 2;
        lefttoright = 0;
      }
      else
      {
        top = 1;
        mid = 2;
        bot = 0;
        lefttoright = 1;
      }
    }

    Gradients gradients(positions);
    Edge topmid(gradients, positions[top], positions[mid]);
    Edge topbot(gradients, positions[top], positions[bot]);
    Edge midbot(gradients, positions[mid], positions[bot]);

    int topy = max((int)positions[top].y, 0);
    int midy = clamp((int)positions[mid].y, 0, OcclusionBuffer::Height);
    int boty = min((int)positions[bot].y, OcclusionBuffer::Height);

    if (lefttoright)
    {
      topbot.x += (topy + 1.0f - positions[top].y) * topbot.xstep;
      topmid.x += (topy + 1.0f - positions[top].y) * topmid.xstep;
      midbot.x += (midy + 1.0f - positions[mid].y) * midbot.xstep;
      topbot.z += (topy + 1.0f - positions[top].y) * topbot.zstep;

      // topbot to topmid
      for(float *row = &buffer[0][0] + topy * OcclusionBuffer::Width, *end = &buffer[0][0] + midy * OcclusionBuffer::Width; row < end; row += OcclusionBuffer::Width)
      {
        int topx = max((int)topbot.x, 0);
        int midx = min((int)topmid.x, OcclusionBuffer::Width);

        float z = topbot.z + (topx + 1.0f - topbot.x) * gradients.dzdx;

        for(float *frag = row + topx, *end = row + midx; frag < end; ++frag)
        {
          if (z < *frag)
            *frag = z;

          z += gradients.dzdx;
        }

        topbot.x += topbot.xstep;
        topmid.x += topmid.xstep;
        topbot.z += topbot.zstep;
      }

      // topbot to midbot
      for(float *row = &buffer[0][0] + midy * OcclusionBuffer::Width, *end = &buffer[0][0] + boty * OcclusionBuffer::Width; row < end; row += OcclusionBuffer::Width)
      {
        int topx = max((int)topbot.x, 0);
        int midx = min((int)midbot.x, OcclusionBuffer::Width);

        float z = topbot.z + (topx + 1.0f - topbot.x) * gradients.dzdx;

        for(float *frag = row + topx, *end = row + midx; frag < end; ++frag)
        {
          if (z < *frag)
            *frag = z;

          z += gradients.dzdx;
        }

        topbot.x += topbot.xstep;
        midbot.x += midbot.xstep;
        topbot.z += topbot.zstep;
      }
    }
    else
    {
      topmid.x += (topy + 1.0f - positions[top].y) * topmid.xstep;
      topbot.x += (topy + 1.0f - positions[top].y) * topbot.xstep;
      midbot.x += (midy + 1.0f - positions[mid].y) * midbot.xstep;
      topmid.z += (topy + 1.0f - positions[top].y) * topmid.zstep;
      midbot.z += (midy + 1.0f - positions[mid].y) * midbot.zstep;

      // topmid to topbot
      for(float *row = &buffer[0][0] + topy * OcclusionBuffer::Width, *end = &buffer[0][0] + midy * OcclusionBuffer::Width; row < end; row += OcclusionBuffer::Width)
      {
        int midx = max((int)topmid.x, 0);
        int topx = min((int)topbot.x, OcclusionBuffer::Width);

        float z = topmid.z + (midx + 1.0f - topmid.x) * gradients.dzdx;

        for(float *frag = row + midx, *end = row + topx; frag < end; ++frag)
        {
          if (z < *frag)
            *frag = z;

          z += gradients.dzdx;
        }

        topmid.x += topmid.xstep;
        topbot.x += topbot.xstep;
        topmid.z += topmid.zstep;
      }

      // midbot to topbot
      for(float *row = &buffer[0][0] + midy * OcclusionBuffer::Width, *end = &buffer[0][0] + boty * OcclusionBuffer::Width; row < end; row += OcclusionBuffer::Width)
      {
        int midx = max((int)midbot.x, 0);
        int topx = min((int)topbot.x, OcclusionBuffer::Width);

        float z = midbot.z + (midx + 1.0f - midbot.x) * gradients.dzdx;

        for(float *frag = row + midx, *end = row + topx; frag < end; ++frag)
        {
          if (z < *frag)
            *frag = z;

          z += gradients.dzdx;
        }

        midbot.x += midbot.xstep;
        topbot.x += topbot.xstep;
        midbot.z += midbot.zstep;
      }
    }
  }


  ///////////////////////// fill_triangle ///////////////////////////////////
  void fill_triangle(float (&buffer)[OcclusionBuffer::Height][OcclusionBuffer::Width], Vec4 *vertices)
  {
    int clipmask[3] = { 0, 0, 0 };

    for(int i = 0; i < 3; ++i)
    {
      if (vertices[i].x > vertices[i].w)
        clipmask[i] |= ClipPosX;

      if (vertices[i].x < -vertices[i].w)
        clipmask[i] |= ClipNegX;

      if (vertices[i].y > vertices[i].w)
        clipmask[i] |= ClipPosY;

      if (vertices[i].y < -vertices[i].w)
        clipmask[i] |= ClipNegY;

      if (vertices[i].z > vertices[i].w)
        clipmask[i] |= ClipPosZ;

      if (vertices[i].z < 0.0f)
        clipmask[i] |= ClipNegZ;
    }

    // all verts outside same clipping plane(s)
    if ((clipmask[0] & clipmask[1] & clipmask[2]) != 0)
      return;

    // near plane cliping to hard
    if ((clipmask[0] | clipmask[1] | clipmask[2]) & ClipNegZ)
      return;

    // cull backface
    if (orientation(vertices[0].xy, vertices[1].xy, vertices[2].xy) <= 0.0f)
      return;

    rasterize(buffer, vertices);
  }

}


//|---------------------- OcclusionBuffer -----------------------------------
//|--------------------------------------------------------------------------

constexpr int OcclusionBuffer::Width;
constexpr int OcclusionBuffer::Height;

///////////////////////// OcclusionBuffer::Constructor //////////////////////
OcclusionBuffer::OcclusionBuffer()
{
  clear();
}


///////////////////////// OcclusionBuffer::clear ////////////////////////////
void OcclusionBuffer::clear()
{
  for(int j = 0; j < Height; ++j)
  {
    for(int i = 0; i < Width; ++i)
    {
      m_buffer[j][i] = 1.0f;
    }
  }
}


///////////////////////// OcclusionBuffer::fill_elements ////////////////////
void OcclusionBuffer::fill_elements(Matrix4f worldview, Vertex const *vertices, uint32_t const *indices, int elementcount)
{
  for(uint32_t const *index = indices, *end = indices + elementcount; index != end; index += 3)
  {
    Vec4 positions[3];

    positions[0] = worldview * Vec4(vertices[*(index+0)].position, 1.0f);
    positions[1] = worldview * Vec4(vertices[*(index+1)].position, 1.0f);
    positions[2] = worldview * Vec4(vertices[*(index+2)].position, 1.0f);

    fill_triangle(m_buffer, positions);
  }
}


///////////////////////// OcclusionBuffer::visible //////////////////////////
bool OcclusionBuffer::visible(Matrix4f worldview, Bound3 const &bound) const
{
  Vec4 corners[8];
  corners[0] = worldview * Vec4(bound.min.x, bound.min.y, bound.min.z, 1.0f);
  corners[1] = worldview * Vec4(bound.max.x, bound.min.y, bound.min.z, 1.0f);
  corners[2] = worldview * Vec4(bound.min.x, bound.max.y, bound.min.z, 1.0f);
  corners[3] = worldview * Vec4(bound.max.x, bound.max.y, bound.min.z, 1.0f);
  corners[4] = worldview * Vec4(bound.min.x, bound.min.y, bound.max.z, 1.0f);
  corners[5] = worldview * Vec4(bound.max.x, bound.min.y, bound.max.z, 1.0f);
  corners[6] = worldview * Vec4(bound.min.x, bound.max.y, bound.max.z, 1.0f);
  corners[7] = worldview * Vec4(bound.max.x, bound.max.y, bound.max.z, 1.0f);

  // intersecting near plane is inconclusive
  for(int i = 0; i < 8; ++i)
  {
    if (corners[i].z <= 0.0f)
      return true;
  }

  float minx = numeric_limits<float>::max();
  float maxx = numeric_limits<float>::lowest();
  float miny = numeric_limits<float>::max();
  float maxy = numeric_limits<float>::lowest();
  float minz = 1.0f;

  for(int i = 0; i < 8; ++i)
  {
    float invw = 1.0f / corners[i].w;

    minx = min(minx, corners[i].x * invw);
    maxx = max(maxx, corners[i].x * invw);
    miny = min(miny, corners[i].y * invw);
    maxy = max(maxy, corners[i].y * invw);
    minz = min(minz, corners[i].z * invw);
  }

  int left = max(int((0.5f * minx + 0.5f) * (Width - 1) - 1.5f), 0);
  int right = min(int((0.5f * maxx + 0.5f) * (Width - 1) + 1.5f), Width);
  int top = max(int((0.5f * miny + 0.5f) * (Height - 1) - 1.5f), 0);
  int bottom = min(int((0.5f * maxy + 0.5f) * (Height- 1) + 1.5f), Height);

  for(float const *row = &m_buffer[0][0] + top * Width, *end = &m_buffer[0][0] + bottom * Width; row < end; row += Width)
  {
    for(float const *frag = row + left, *end = row + right; frag < end; ++frag)
    {
      if (minz <= *frag)
        return true;
    }
  }

  return false;
}
