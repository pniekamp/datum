//
// Datum - ibl
//

#pragma once

#include "hdr.h"

void image_buildmips_cube_ibl(int width, int height, int levels, void *bits);

void image_pack_cube_ibl(HDRImage const &image, int width, int height, int levels, void *bits);

void image_pack_envbrdf(int width, int height, void *bits);

void image_pack_watercolor(lml::Color3 const &deepcolor, lml::Color3 const &shallowcolor, float depthscale, lml::Color3 const &fresnelcolor, float fresnelbias, float fresnelpower, int width, int height, void *bits);
