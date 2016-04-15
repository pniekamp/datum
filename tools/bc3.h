//
// Datum - bc3 compression
//

#pragma once

#include <cstdint>

//|---------------------- BC3 -----------------------------------------------
//|--------------------------------------------------------------------------

struct BC3
{
  uint8_t alpha0;
  uint8_t alpha1;
  uint8_t bitmap[6];

  uint16_t color0;
  uint16_t color1;
  uint32_t indices;
};

void encode(uint32_t block[16], BC3 *result);
