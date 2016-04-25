//
// Datum - occlusion
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "mesh.h"
#include "math/vec.h"


//|---------------------- OcclusionBuffer -----------------------------------
//|--------------------------------------------------------------------------

class OcclusionBuffer
{
  public:
    OcclusionBuffer();

    static const int kWidth = 256;
    static const int kHeight = 144;

    void clear();

    void fill_elements(lml::Matrix4f worldview, Vertex const *vertices, uint32_t const *indices, int elementcount);

  public:

    bool visible(lml::Matrix4f worldview, lml::Bound3 const &bound) const;

  private:

    float m_buffer[kHeight][kWidth];
};


//|///////////////////// visible ////////////////////////////////////////////
inline bool visible(lml::Matrix4f worldview, OcclusionBuffer const &buffer, lml::Bound3 const &bound)
{
  return buffer.visible(worldview, bound);
}
