//
// Datum - asset packer
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "assetpacker.h"
#include "datum/math.h"
#include <leap/lz4.h>
#include "bc3.h"
#include <cassert>

using namespace std;
using namespace lml;
using namespace leap::crypto;

namespace
{
  float alpha_coverage(float alpharef, int width, int height, void *bits)
  {
    uint32_t *pixel = (uint32_t*)bits;

    int coverage = 0;

    for(int i = 0; i < width*height; ++i)
    {
      if (rgba(*pixel).a > alpharef)
        ++coverage;
    }

    return coverage * 1.0f/(width*height);
  }

  float alpha_coverage_scale(float alpharef, float coverage, int width, int height, void *bits)
  {
    float lo = 0.0f;
    float hi = alpharef;

    for(int i = 0; i < 10; ++i)
    {
      float k = (0.5f*lo + 0.5f*hi);

      if (alpha_coverage(k, width, height, bits) < coverage - 0.05)
        hi = k;
      else
        lo = k;
    }

    return alpharef / (0.5f*lo + 0.5f*hi);
  }
}


///////////////////////// write_header //////////////////////////////////////
void write_header(ostream &fout)
{
  fout << '\xD9';
  fout << 'S' << 'V' << 'A';
  fout << '\x0D' << '\x0A';
  fout << '\x1A';
  fout << '\x0A';
}


///////////////////////// write_chunk ///////////////////////////////////////
void write_chunk(ostream &fout, const char type[4], uint32_t length, void const *data)
{
  uint32_t checksum = 0;

  for(size_t i = 0; i < length; ++i)
    checksum ^= static_cast<uint8_t const*>(data)[i] << (i % 4);

  fout.write((char*)&length, sizeof(length));
  fout.write((char*)type, 4);
  fout.write((char*)data, length);
  fout.write((char*)&checksum, sizeof(checksum));
}


///////////////////////// write_compressed_chunk ////////////////////////////
void write_compressed_chunk(ostream &fout, const char type[4], uint32_t length, void const *data)
{
  vector<uint8_t> payload;

  while(length)
  {
    PackBlock block;

    size_t bytes = length;
    block.size = lz4_compress(data, block.data, &bytes, sizeof(block.data));

    if (bytes == length)
      payload.insert(payload.end(), (uint8_t*)&block, (uint8_t*)&block + sizeof(uint32_t) + block.size);
    else
      payload.insert(payload.end(), (uint8_t*)&block, (uint8_t*)&block + sizeof(block));

    length -= bytes;
    data = (void const *)((size_t)data + bytes);
  }

  write_chunk(fout, type, payload.size(), payload.data());
}


///////////////////////// write_catl_asset //////////////////////////////////
uint32_t write_catl_asset(ostream &fout, uint32_t id)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  write_chunk(fout, "CATL", 0, nullptr);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_text_asset //////////////////////////////////
uint32_t write_text_asset(ostream &fout, uint32_t id, std::string const &str)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackTextHeader shdr = { (uint32_t)str.length() + 1, (size_t)fout.tellp() + sizeof(shdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "TEXT", sizeof(shdr), &shdr);

  write_chunk(fout, "DATA", str.length() + 1, str.c_str());

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_text_asset //////////////////////////////////
uint32_t write_text_asset(ostream &fout, uint32_t id, std::vector<uint8_t> const &str)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackTextHeader shdr = { (uint32_t)str.size(), (size_t)fout.tellp() + sizeof(shdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "TEXT", sizeof(shdr), &shdr);

  write_chunk(fout, "DATA", str.size(), str.data());

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_imag_asset //////////////////////////////////
uint32_t write_imag_asset(ostream &fout, uint32_t id, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, void const *bits, float alignx, float aligny)
{
  assert(layers > 0);
  assert(levels > 0 && levels <= (uint32_t)image_maxlevels(width, height));

  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  uint32_t datasize = sizeof(PackImagePayload);

  switch(((PackImagePayload*)bits)->compression)
  {
    case PackImagePayload::none:
      datasize += image_datasize(width, height, layers, levels);
      break;

    case PackImagePayload::bc3:
      datasize += image_datasize_bc3(width, height, layers, levels);
      break;

    default:
      assert(false);
  }

  PackImageHeader ihdr = { width, height, layers, levels, alignx, aligny, datasize, (size_t)fout.tellp() + sizeof(ihdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "IMAG", sizeof(ihdr), &ihdr);

  write_chunk(fout, "DATA", datasize, bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_mesh_asset //////////////////////////////////
uint32_t write_mesh_asset(ostream &fout, uint32_t id, vector<PackVertex> const &vertices, vector<uint32_t> const &indices)
{
  Bound3 bound = bound_limits<Bound3>::min();

  for(auto &vertex : vertices)
  {
    bound = expand(bound, Vec3(vertex.position[0], vertex.position[1], vertex.position[2]));
  }

  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  size_t datasize = vertices.size()*sizeof(PackVertex) + indices.size()*sizeof(uint32_t);

  PackMeshHeader vhdr = { (uint32_t)vertices.size(), (uint32_t)indices.size(), bound.min.x, bound.min.y, bound.min.z, bound.max.x, bound.max.y, bound.max.z, (size_t)fout.tellp() + sizeof(vhdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "MESH", sizeof(vhdr), &vhdr);

  vector<uint8_t> payload(datasize);

  auto vertextable = reinterpret_cast<PackVertex*>(payload.data());
  auto indextable = reinterpret_cast<uint32_t*>(payload.data() + vertices.size() * sizeof(PackVertex));

  memcpy(vertextable, &vertices[0], vertices.size()*sizeof(PackVertex));
  memcpy(indextable, &indices[0], indices.size()*sizeof(uint32_t));

  write_chunk(fout, "DATA", payload.size(), payload.data());

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_matl_asset //////////////////////////////////
uint32_t write_matl_asset(ostream &fout, uint32_t id, Color3 albedocolor, uint32_t albedomap, Color3 specularintensity, float specularexponent, uint32_t specularmap, uint32_t normalmap)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackMaterialHeader mhdr = { (size_t)fout.tellp() + sizeof(mhdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "MATL", sizeof(mhdr), &mhdr);

  PackMaterialPayload matl;

  matl.albedocolor[0] = albedocolor.r;
  matl.albedocolor[1] = albedocolor.g;
  matl.albedocolor[2] = albedocolor.b;
  matl.albedomap = albedomap;
  matl.specularintensity[0] = specularintensity.r;
  matl.specularintensity[1] = specularintensity.g;
  matl.specularintensity[2] = specularintensity.b;
  matl.specularexponent = max(1.0f, specularexponent);
  matl.specularmap = specularmap;
  matl.normalmap = normalmap;

  write_chunk(fout, "DATA", sizeof(matl), &matl);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// image_maxlevels ///////////////////////////////////
int image_maxlevels(int width, int height)
{
  int levels = 1;
  for(int i = 0; i < 16; ++i)
  {
    if ((width >> i) == 1 || (height >> i) == 1)
      break;

    ++levels;
  }

  return levels;
}


///////////////////////// image_datasize ////////////////////////////////////
size_t image_datasize(int width, int height, int layers, int levels)
{
  size_t size = 0;
  for(int i = 0; i < levels; ++i)
  {
    size += (width >> i) * (height >> i) * layers * sizeof(uint32_t);
  }

  return size;
}


///////////////////////// image_datasize ////////////////////////////////////
size_t image_datasize_bc3(int width, int height, int layers, int levels)
{
  size_t size = 0;
  for(int i = 0; i < levels; ++i)
  {
    size += (((width >> i)+3)/4) * (((height >> i)+3)/4) * layers * sizeof(BC3);
  }

  return size;
}


///////////////////////// image_buildmips ///////////////////////////////////
void image_buildmips_rgb(int width, int height, int layers, int levels, void *bits)
{
  uint32_t *src = (uint32_t*)((char*)bits + sizeof(PackImagePayload));
  uint32_t *dst = src + width * height * layers;

  for(int level = 1; level < levels; ++level)
  {
    for(int layer = 0; layer < layers; ++layer)
    {
      for(int y = 0, end = height >> 1; y < end; ++y)
      {
        for(int x = 0, end = width >> 1; x < end; ++x)
        {
          Color4 tl = rgba(*(src + ((y << 1) + 0)*width + ((x << 1) + 0)));
          Color4 tr = rgba(*(src + ((y << 1) + 0)*width + ((x << 1) + 1)));
          Color4 bl = rgba(*(src + ((y << 1) + 1)*width + ((x << 1) + 0)));
          Color4 br = rgba(*(src + ((y << 1) + 1)*width + ((x << 1) + 1)));

          *dst++ = rgba((tl + tr + bl + br) / 4);
        }
      }

      src += width * height;
    }

    width /= 2;
    height /= 2;
  }
}


///////////////////////// image_buildmips ///////////////////////////////////
void image_buildmips_srgb(int width, int height, int layers, int levels, void *bits)
{
  uint32_t *src = (uint32_t*)((char*)bits + sizeof(PackImagePayload));
  uint32_t *dst = src + width * height * layers;

  for(int level = 1; level < levels; ++level)
  {
    for(int layer = 0; layer < layers; ++layer)
    {
      for(int y = 0, end = height >> 1; y < end; ++y)
      {
        for(int x = 0, end = width >> 1; x < end; ++x)
        {
          Color4 tl = srgba(*(src + ((y << 1) + 0)*width + ((x << 1) + 0)));
          Color4 tr = srgba(*(src + ((y << 1) + 0)*width + ((x << 1) + 1)));
          Color4 bl = srgba(*(src + ((y << 1) + 1)*width + ((x << 1) + 0)));
          Color4 br = srgba(*(src + ((y << 1) + 1)*width + ((x << 1) + 1)));

          *dst++ = srgba((tl + tr + bl + br) / 4);
        }
      }

      src += width * height;
    }

    width /= 2;
    height /= 2;
  }
}


///////////////////////// image_buildmips ///////////////////////////////////
void image_buildmips_srgb_a(float alpharef, int width, int height, int layers, int levels, void *bits)
{
  uint32_t *src = (uint32_t*)((char*)bits + sizeof(PackImagePayload));
  uint32_t *dst = src + width * height * layers;

  for(int level = 1; level < levels; ++level)
  {
    for(int layer = 0; layer < layers; ++layer)
    {
      uint32_t *pixel = dst;

      for(int y = 0, end = height >> 1; y < end; ++y)
      {
        for(int x = 0, end = width >> 1; x < end; ++x)
        {
          Color4 tl = srgba(*(src + ((y << 1) + 0)*width + ((x << 1) + 0)));
          Color4 tr = srgba(*(src + ((y << 1) + 0)*width + ((x << 1) + 1)));
          Color4 bl = srgba(*(src + ((y << 1) + 1)*width + ((x << 1) + 0)));
          Color4 br = srgba(*(src + ((y << 1) + 1)*width + ((x << 1) + 1)));

          *pixel++ = srgba((tl + tr + bl + br) / 4);
        }
      }

      float coverage = alpha_coverage(alpharef, width, height, src);

      float alphascale = alpha_coverage_scale(alpharef, coverage, width >> 1, height >> 1, dst);

      for(int y = 0, end = height >> 1; y < end; ++y)
      {
        for(int x = 0, end = width >> 1; x < end; ++x)
        {
          uint32_t pixel = *dst;

          *dst++ = (pixel & 0x00ffffff) | (uint8_t)min(((pixel >> 24) & 0xff) * alphascale, 255.0f) << 24;
        }
      }

      src += width * height;
    }

    width /= 2;
    height /= 2;
  }
}


///////////////////////// image_premultiply /////////////////////////////////
void image_premultiply_srgb(int width, int height, int layers, int levels, void *bits)
{
  uint32_t *src = (uint32_t*)((char*)bits + sizeof(PackImagePayload));
  uint32_t *dst = src;

  size_t datasize = image_datasize(width, height, layers, levels);

  for(size_t i = 0; i < datasize / sizeof(uint32_t); ++i)
  {
    *dst++ = srgba(premultiply(srgba(*src++)));
  }
}


///////////////////////// image_compress_bc3 ////////////////////////////////
void image_compress_bc3(int width, int height, int layers, int levels, void *bits)
{
  uint32_t *src = (uint32_t*)((char*)bits + sizeof(PackImagePayload));
  BC3 *dst = (BC3*)src;

  for(int level = 0; level < levels; ++level)
  {
    for(int layer = 0; layer < layers; ++layer)
    {
      int count = 0;
      vector<BC3> blocks(((width+3)/4) * ((height+3)/4));

      for(int y = 0; y < height; y += 4)
      {
        for(int x = 0; x < width; x += 4)
        {
          uint32_t block[16];
          for(int k = 0; k < min(4, height - y); ++k)
          {
            memcpy(&block[k*4], src + (y+k)*width + x, min(4, width - x)*sizeof(uint32_t));
          }

          encode(block, &blocks[count++]);
        }
      }

      memcpy(dst, blocks.data(), count * sizeof(BC3));

      src += width * height;
      dst += count;
    }

    width /= 2;
    height /= 2;
  }

  ((PackImagePayload*)bits)->compression = PackImagePayload::bc3;
}
