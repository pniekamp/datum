//
// Datum - hdr loader
//

#pragma once

#include "datum/math.h"
#include <vector>
#include <string>

//|---------------------- HDRImage ------------------------------------------
//|--------------------------------------------------------------------------

class HDRImage
{
  public:

    int width;
    int height;
    float exposure = 1.0f;

    std::vector<lml::Color4> bits;

    lml::Color4 sample(int i, int j) const;

    lml::Color4 sample(lml::Vec2 const &texcoords) const;
    lml::Color4 sample(lml::Vec3 const &texcoords) const;

    lml::Color4 sample(lml::Vec2 const &texcoords, lml::Vec2 const &area) const;
    lml::Color4 sample(lml::Vec3 const &texcoords, lml::Vec2 const &area) const;
};

HDRImage load_hdr(std::string const &path);

void image_buildmips_cube(int width, int height, int levels, void *bits);

void image_pack_cube(HDRImage const &image, int width, int height, int levels, void *bits);
