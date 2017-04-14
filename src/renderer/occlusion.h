//
// Datum - occlusion
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "datum/math.h"

//|---------------------- OcclusionBuffer -----------------------------------
//|--------------------------------------------------------------------------

class OcclusionBuffer
{
  public:

    struct Vertex
    {
      lml::Vec3 position;
    };

  public:
    OcclusionBuffer();

    static constexpr int Width = 256;
    static constexpr int Height = 144;

    void clear();

    void fill_elements(lml::Matrix4f worldview, Vertex const *vertices, uint32_t const *indices, int elementcount);

  public:

    bool visible(lml::Matrix4f worldview, lml::Bound3 const &bound) const;

  private:

    float m_buffer[Height][Width];
};


//|///////////////////// visible ////////////////////////////////////////////
inline bool visible(lml::Matrix4f worldview, OcclusionBuffer const &buffer, lml::Bound3 const &bound)
{
  return buffer.visible(worldview, bound);
}
