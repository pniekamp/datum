//
// Datum - hdr loader
//

#include "hdr.h"
#include "datum/math.h"
#include "assetpacker.h"
#include <iostream>
#include <fstream>

using namespace std;
using namespace lml;
using namespace leap;


//|---------------------- HDRImage ------------------------------------------
//|--------------------------------------------------------------------------

//|///////////////////// sample /////////////////////////////////////////////
Color4 HDRImage::sample(int i, int j) const
{
  return bits[j * width + i];
}


//|///////////////////// sample /////////////////////////////////////////////
Color4 HDRImage::sample(Vec2 const &texcoords) const
{
  float i, j;
  auto u = modf(fmod2(texcoords.x, 1.0f) * (width - 1), &i);
  auto v = modf(fmod2(texcoords.y, 1.0f) * (height - 1), &j);

  return lerp(lerp(sample(i, j), sample(i+1, j), u), lerp(sample(i, j+1), sample(i+1, j+1), u), v);

}


//|///////////////////// sample /////////////////////////////////////////////
Color4 HDRImage::sample(Vec2 const &texcoords, Vec2 const &area) const
{
  Color4 sum(0, 0, 0);
  float totalweight = 0;

  for(float y = texcoords.y - 0.5*area.y; y < texcoords.y + 0.5*area.y; y += 1.0f/height)
  {
    for(float x = texcoords.x - 0.5*area.x; x < texcoords.x + 0.5*area.x; x += 1.0f/width)
    {
      sum += sample(Vec2(x, y));

      totalweight += 1.0f;
    }
  }

  return sum / totalweight;
}


//|///////////////////// sample /////////////////////////////////////////////
Color4 HDRImage::sample(Vec3 const &texcoords) const
{
  return sample(Vec2(pi<float>()/2 + atan2(texcoords.x, texcoords.z) / (2*pi<float>()), acos(texcoords.y) / pi<float>()));
}


//|///////////////////// sample /////////////////////////////////////////////
Color4 HDRImage::sample(Vec3 const &texcoords, Vec2 const &area) const
{
  return sample(Vec2(pi<float>()/2 + atan2(texcoords.x, texcoords.z) / (2*pi<float>()), acos(texcoords.y) / pi<float>()), area);
}


//|///////////////////// load_hdr ///////////////////////////////////////////
HDRImage load_hdr(string const &path)
{
  HDRImage image = {};

  ifstream fin(path, ios_base::in | ios_base::binary);
  if (!fin)
    throw runtime_error("Unable to open file: " + path);

  string line;

  while (getline(fin, line))
  {
    line = trim(line);

    if (line.empty() || line[0] == '#')
      continue;

    if (tolower(line.substr(0, 6)) == "format")
    {
      vector<string> fields = split(line, "=");

      if (fields.size() != 2 && tolower(fields[1]) != "32-bit_rle_rgbe")
        throw runtime_error("Unsupported hdr file format");
    }

    if (tolower(line.substr(0, 8)) == "exposure")
    {
      vector<string> fields = split(line, "=");

      image.exposure = ato<float>(fields[1]);
    }

    if (line[0] == '-' || line[0] == '+')
    {
      vector<string> fields = split(line);

      if (fields[0] != "-Y" || fields[2] != "+X")
        throw runtime_error("Unsupported hdr file dimensions");

      image.width = ato<float>(fields[3]);
      image.height = ato<float>(fields[1]);

      break;
    }
  }

  image.bits.resize(image.width * image.height);

  for(int y = 0; y < image.height; ++y)
  {
    char buffer[4][32768];

    fin.read(&buffer[0][0], 4);

    if (buffer[0][0] != 2 || buffer[0][1] != 2 || (((uint8_t)(buffer[0][2]) << 8) | ((uint8_t)(buffer[0][3]) << 0)) != image.width)
      throw runtime_error("hdr parse error");

    for(int k = 0; k < 4; ++k)
    {
      int position = 0;

      while (position < image.width)
      {
        int count = fin.get();

        if (count > 128)
        {
          count -= 128;
          memset(&buffer[k][position], fin.get(), count);
        }
        else
        {
          fin.read(&buffer[k][position], count);
        }

        position += count;
      }
    }

    for(int x = 0; x < image.width; ++x)
    {
      auto r = (uint8_t)(buffer[0][x]) / 255.0;
      auto g = (uint8_t)(buffer[1][x]) / 255.0;
      auto b = (uint8_t)(buffer[2][x]) / 255.0;
      auto e = (uint8_t)(buffer[3][x]) - 128;

      image.bits[y * image.width + x] = Color4(r * exp2(e), g * exp2(e), b * exp2(e), 1.0f);
    }
  }

  return image;
}


///////////////////////// image_blend_edges /////////////////////////////////
void image_blend_edges(int width, int height, int levels, void *bits)
{
  uint32_t *img = (uint32_t*)((char*)bits + sizeof(PackImagePayload));

  for(int level = 0; level < levels && width > 1 && height > 1; ++level)
  {
    auto tex = [=](int x, int y, int z) -> uint32_t& { return *(img + z*width*height + y*width + x); };

    for(int k = 0; k < height; ++k)
    {
      auto a = rgbe(tex(width - 2, k, 4));
      auto b = rgbe(tex(width - 1, k, 4));
      auto c = rgbe(tex(0, k, 0));
      auto d = rgbe(tex(1, k, 0));

      tex(width - 1, k, 4) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(0, k, 0) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < height; ++k)
    {
      auto a = rgbe(tex(width - 2, k, 0));
      auto b = rgbe(tex(width - 1, k, 0));
      auto c = rgbe(tex(0, k, 5));
      auto d = rgbe(tex(1, k, 5));

      tex(width - 1, k, 0) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(0, k, 5) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < height; ++k)
    {
      auto a = rgbe(tex(width - 2, k, 5));
      auto b = rgbe(tex(width - 1, k, 5));
      auto c = rgbe(tex(0, k, 1));
      auto d = rgbe(tex(1, k, 1));

      tex(width - 1, k, 5) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(0, k, 1) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < height; ++k)
    {
      auto a = rgbe(tex(width - 2, k, 1));
      auto b = rgbe(tex(width - 1, k, 1));
      auto c = rgbe(tex(0, k, 4));
      auto d = rgbe(tex(1, k, 4));

      tex(width - 1, k, 1) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(0, k, 4) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < width; ++k)
    {
      auto a = rgbe(tex(k, height - 2, 4));
      auto b = rgbe(tex(k, height - 1, 4));
      auto c = rgbe(tex(k, 0, 3));
      auto d = rgbe(tex(k, 1, 3));

      tex(k, height - 1, 4) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(k, 0, 3) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < width; ++k)
    {
      auto a = rgbe(tex(k, height - 2, 3));
      auto b = rgbe(tex(k, height - 1, 3));
      auto c = rgbe(tex(width - 1 - k, height - 1, 5));
      auto d = rgbe(tex(width - 1 - k, height - 2, 5));

      tex(k, height - 1, 3) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(width - 1 - k, height - 1, 5) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < width; ++k)
    {
      auto a = rgbe(tex(k, 1, 5));
      auto b = rgbe(tex(k, 0, 5));
      auto c = rgbe(tex(width - 1 - k, 0, 2));
      auto d = rgbe(tex(width - 1 - k, 1, 2));

      tex(k, 0, 5) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(width - 1 - k, 0, 2) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < width; ++k)
    {
      auto a = rgbe(tex(k, height - 2, 2));
      auto b = rgbe(tex(k, height - 1, 2));
      auto c = rgbe(tex(k, 0, 4));
      auto d = rgbe(tex(k, 1, 4));

      tex(k, height - 1, 2) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(k, 0, 4) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < min(width, height); ++k)
    {
      auto a = rgbe(tex(k, height - 2, 0));
      auto b = rgbe(tex(k, height - 1, 0));
      auto c = rgbe(tex(width - 1, k, 3));
      auto d = rgbe(tex(width - 2, k, 3));

      tex(k, height - 1, 0) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(width - 1, k, 3) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < min(width, height); ++k)
    {
      auto a = rgbe(tex(1, k, 3));
      auto b = rgbe(tex(0, k, 3));
      auto c = rgbe(tex(width - 1 - k, height - 1, 1));
      auto d = rgbe(tex(width - 1 - k, height - 2, 1));

      tex(0, k, 3) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(width - 1 - k, height - 1, 1) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < min(width, height); ++k)
    {
      auto a = rgbe(tex(k, 1, 1));
      auto b = rgbe(tex(k, 0, 1));
      auto c = rgbe(tex(0, k, 2));
      auto d = rgbe(tex(1, k, 2));

      tex(k, 0, 1) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(0, k, 2) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    for(int k = 0; k < min(width, height); ++k)
    {
      auto a = rgbe(tex(width - 2, k, 2));
      auto b = rgbe(tex(width - 1, k, 2));
      auto c = rgbe(tex(width - 1 - k, 0, 0));
      auto d = rgbe(tex(width - 1 - k, 1, 0));

      tex(width - 1, k, 2) = rgbe(0.3f * a + 0.4f * b + 0.3f * c);
      tex(width - 1 - k, 0, 0) = rgbe(0.3f * b + 0.4f * c + 0.3f * d);
    }

    img += width * height * 6;

    width /= 2;
    height /= 2;
  }
}


///////////////////////// image_buildmips_cube //////////////////////////////
void image_buildmips_cube(int width, int height, int levels, void *bits)
{
  image_buildmips_rgbe(width, height, 6, levels, bits);

  image_blend_edges(width, height, levels, bits);
}


///////////////////////// image_pack_cube ///////////////////////////////////
void image_pack_cube(HDRImage const &image, int width, int height, int levels, void *bits)
{
  uint32_t *dst = (uint32_t*)((char*)bits + sizeof(PackImagePayload));

  Transform transforms[] =
  {
    Transform::rotation(Vec3(0, 1, 0), pi<float>()/2),  // right
    Transform::rotation(Vec3(0, 1, 0), -pi<float>()/2), // left
    Transform::rotation(Vec3(1, 0, 0), pi<float>()/2),  // down
    Transform::rotation(Vec3(1, 0, 0), -pi<float>()/2), // up
    Transform::rotation(Vec3(0, 1, 0), 0),              // forward
    Transform::rotation(Vec3(0, 1, 0), pi<float>()),    // back
  };

  Vec2 area = Vec2(1.0f / (4.0f * width), 1.0f / (2.0f * height));

  for(auto &transform : transforms)
  {
    for(int y = 0; y < height; ++y)
    {
      for(int x = 0; x < width; ++x)
      {
        *dst++ = rgbe(image.sample(transform * normalise(Vec3(2 * (x + 0.5f)/width - 1, 2 * (y + 0.5f)/height - 1, 1.0)), area));
      }
    }
  }

  image_buildmips_cube(width, height, levels, bits);
}