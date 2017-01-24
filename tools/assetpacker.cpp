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
#include <numeric>
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
  vector<char> payload;

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
uint32_t write_catl_asset(ostream &fout, uint32_t id, uint32_t magic, uint32_t version)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackCalalogHeader shdr = { magic, version, 0, (size_t)fout.tellp() + sizeof(shdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "CATL", sizeof(shdr), &shdr);

//  write_chunk(fout, "DATA", length, data);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_text_asset //////////////////////////////////
uint32_t write_text_asset(ostream &fout, uint32_t id, uint32_t length, void const *data)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackTextHeader shdr = { length, (size_t)fout.tellp() + sizeof(shdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "TEXT", sizeof(shdr), &shdr);

  write_chunk(fout, "DATA", length, data);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_imag_asset //////////////////////////////////
uint32_t write_imag_asset(ostream &fout, uint32_t id, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, uint32_t format, void const *bits)
{
  assert(layers > 0);
  assert(levels > 0 && levels <= (uint32_t)image_maxlevels(width, height));

  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  uint32_t datasize = 0;

  switch(format)
  {
    case PackImageHeader::rgba:
    case PackImageHeader::rgbe:
      datasize += image_datasize(width, height, layers, levels);
      break;

    case PackImageHeader::rgba_bc3:
      datasize += image_datasize_bc3(width, height, layers, levels);
      break;

    default:
      assert(false);
  }

  PackImageHeader ihdr = { width, height, layers, levels, format, datasize, (size_t)fout.tellp() + sizeof(ihdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "IMAG", sizeof(ihdr), &ihdr);

  write_chunk(fout, "DATA", datasize, bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_font_asset //////////////////////////////////
uint32_t write_font_asset(ostream &fout, uint32_t id, uint32_t ascent, uint32_t descent, uint32_t leading, uint32_t glyphcount, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackFontHeader fhdr = { ascent, descent, leading, glyphcount, (size_t)fout.tellp() + sizeof(fhdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "FONT", sizeof(fhdr), &fhdr);

  write_chunk(fout, "DATA", pack_payload_size(fhdr), bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_font_asset //////////////////////////////////
uint32_t write_font_asset(ostream &fout, uint32_t id, uint32_t ascent, uint32_t descent, uint32_t leading, uint32_t glyphcount, uint32_t glyphatlas, vector<uint16_t> const &x, vector<uint16_t> const &y, vector<uint16_t> const &width, vector<uint16_t> const &height, vector<int16_t> const &offsetx, vector<int16_t> const &offsety, vector<uint8_t> const &advance)
{
  vector<char> payload(sizeof(uint32_t) + 6*glyphcount*sizeof(uint16_t) + glyphcount*glyphcount*sizeof(uint8_t));

  reinterpret_cast<PackFontPayload*>(payload.data())->glyphatlas = glyphatlas;

  auto xtable = const_cast<uint16_t*>(PackFontPayload::xtable(payload.data(), glyphcount));
  auto ytable = const_cast<uint16_t*>(PackFontPayload::ytable(payload.data(), glyphcount));
  auto widthtable = const_cast<uint16_t*>(PackFontPayload::widthtable(payload.data(), glyphcount));
  auto heighttable = const_cast<uint16_t*>(PackFontPayload::heighttable(payload.data(), glyphcount));
  auto offsetxtable = const_cast<int16_t*>(PackFontPayload::offsetxtable(payload.data(), glyphcount));
  auto offsetytable = const_cast<int16_t*>(PackFontPayload::offsetytable(payload.data(), glyphcount));
  auto advancetable = const_cast<uint8_t*>(PackFontPayload::advancetable(payload.data(), glyphcount));

  memcpy(xtable, x.data(), x.size()*sizeof(uint16_t));
  memcpy(ytable, y.data(), y.size()*sizeof(uint16_t));
  memcpy(widthtable, width.data(), width.size()*sizeof(uint16_t));
  memcpy(heighttable, height.data(), height.size()*sizeof(uint16_t));
  memcpy(offsetxtable, offsetx.data(), offsetx.size()*sizeof(int16_t));
  memcpy(offsetytable, offsety.data(), offsety.size()*sizeof(int16_t));
  memcpy(advancetable, advance.data(), advance.size()*sizeof(uint8_t));

  write_font_asset(fout, id, ascent, descent, leading, glyphcount, payload.data());

  return id + 1;
}


///////////////////////// write_mesh_asset //////////////////////////////////
uint32_t write_mesh_asset(ostream &fout, uint32_t id, uint32_t vertexcount, uint32_t indexcount, Bound3 const &bound, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackMeshHeader mesh = { vertexcount, indexcount, bound.min.x, bound.min.y, bound.min.z, bound.max.x, bound.max.y, bound.max.z, (size_t)fout.tellp() + sizeof(mesh) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "MESH", sizeof(mesh), &mesh);

  write_chunk(fout, "DATA", pack_payload_size(mesh), bits);

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

  vector<char> payload(vertices.size()*sizeof(PackVertex) + indices.size()*sizeof(uint32_t));

  auto vertextable = const_cast<PackVertex*>(PackMeshPayload::vertextable(payload.data(), vertices.size(), indices.size()));
  auto indextable = const_cast<uint32_t*>(PackMeshPayload::indextable(payload.data(), vertices.size(), indices.size()));

  memcpy(vertextable, vertices.data(), vertices.size()*sizeof(PackVertex));
  memcpy(indextable, indices.data(), indices.size()*sizeof(uint32_t));

  write_mesh_asset(fout, id, vertices.size(), indices.size(), bound, payload.data());

  return id + 1;
}


///////////////////////// write_matl_asset //////////////////////////////////
uint32_t write_matl_asset(ostream &fout, uint32_t id, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackMaterialHeader mhdr = { (size_t)fout.tellp() + sizeof(mhdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "MATL", sizeof(mhdr), &mhdr);

  write_chunk(fout, "DATA", sizeof(PackMaterialPayload), bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_matl_asset //////////////////////////////////
uint32_t write_matl_asset(ostream &fout, uint32_t id, Color3 const &color, float metalness, float roughness, float reflectivity, float emissive, uint32_t albedomap, uint32_t specularmap, uint32_t normalmap)
{
  PackMaterialPayload matl;

  matl.color[0] = color.r;
  matl.color[1] = color.g;
  matl.color[2] = color.b;
  matl.metalness = metalness;
  matl.roughness = roughness;
  matl.reflectivity = reflectivity;
  matl.emissive = emissive;
  matl.albedomap = albedomap;
  matl.specularmap = specularmap;
  matl.normalmap = normalmap;

  write_matl_asset(fout, id, &matl);

  return id + 1;
}


///////////////////////// write_modl_asset //////////////////////////////////
uint32_t write_modl_asset(ostream &fout, uint32_t id, uint32_t texturecount, uint32_t materialcount, uint32_t meshcount, uint32_t instancecount, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackModelHeader modl = { texturecount, materialcount, meshcount, instancecount, (size_t)fout.tellp() + sizeof(modl) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "MODL", sizeof(modl), &modl);

  write_chunk(fout, "DATA", pack_payload_size(modl), bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_modl_asset //////////////////////////////////
uint32_t write_modl_asset(ostream &fout, uint32_t id, vector<PackModelPayload::Texture> const &textures, vector<PackModelPayload::Material> const &materials, vector<PackModelPayload::Mesh> const &meshes, vector<PackModelPayload::Instance> const &instances)
{
  vector<char> payload(textures.size()*sizeof(PackModelPayload::Texture) + materials.size()*sizeof(PackModelPayload::Material) + meshes.size()*sizeof(PackModelPayload::Mesh) + instances.size()*sizeof(PackModelPayload::Instance));

  auto texturetable = const_cast<PackModelPayload::Texture*>(PackModelPayload::texturetable(payload.data(), textures.size(), materials.size(), meshes.size(), instances.size()));
  auto materialtable = const_cast<PackModelPayload::Material*>(PackModelPayload::materialtable(payload.data(), textures.size(), materials.size(), meshes.size(), instances.size()));
  auto meshtable = const_cast<PackModelPayload::Mesh*>(PackModelPayload::meshtable(payload.data(), textures.size(), materials.size(), meshes.size(), instances.size()));
  auto instancetable = const_cast<PackModelPayload::Instance*>(PackModelPayload::instancetable(payload.data(), textures.size(), materials.size(), meshes.size(), instances.size()));

  memcpy(texturetable, textures.data(), textures.size()*sizeof(PackModelPayload::Texture));
  memcpy(materialtable, materials.data(), materials.size()*sizeof(PackModelPayload::Material));
  memcpy(meshtable, meshes.data(), meshes.size()*sizeof(PackModelPayload::Mesh));
  memcpy(instancetable, instances.data(), instances.size()*sizeof(PackModelPayload::Instance));

  write_modl_asset(fout, id, textures.size(), materials.size(), meshes.size(), instances.size(), payload.data());

  return id + 1;
}


///////////////////////// write_ptsm_asset //////////////////////////////////
uint32_t write_ptsm_asset(ostream &fout, uint32_t id, Bound3 const &bound, uint32_t maxparticles, uint32_t emittercount, uint32_t emitterssize, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackParticleSystemHeader ptsm = { bound.min.x, bound.min.y, bound.min.z, bound.max.x, bound.max.y, bound.max.z, maxparticles, emittercount, emitterssize, (size_t)fout.tellp() + sizeof(ptsm) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "PTSM", sizeof(ptsm), &ptsm);

  write_chunk(fout, "DATA", pack_payload_size(ptsm), bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_ptsm_asset //////////////////////////////////
uint32_t write_ptsm_asset(ostream &fout, uint32_t id, Bound3 const &bound, uint32_t spritesheet, uint32_t maxparticles, vector<vector<uint8_t>> const &emitters)
{
  uint32_t emittercount = emitters.size();
  uint32_t emitterssize = accumulate(emitters.begin(), emitters.end(), 0, [](int i, auto &k) { return i + k.size(); });

  vector<char> payload(sizeof(PackParticleSystemPayload) + emitterssize);

  reinterpret_cast<PackParticleSystemPayload*>(payload.data())->spritesheet = spritesheet;

  size_t cursor = 0;
  for(size_t i = 0; i < emittercount; ++i)
  {
    memcpy(const_cast<uint8_t*>(PackParticleSystemPayload::emitter(payload.data(), cursor)), emitters[i].data(), emitters[i].size());

    cursor += emitters[i].size();
  }

  write_ptsm_asset(fout, id, bound, maxparticles, emittercount, emitterssize, payload.data());

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
  uint32_t *src = (uint32_t*)bits;
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
void image_buildmips_rgbe(int width, int height, int layers, int levels, void *bits)
{
  uint32_t *src = (uint32_t*)bits;
  uint32_t *dst = src + width * height * layers;

  for(int level = 1; level < levels; ++level)
  {
    for(int layer = 0; layer < layers; ++layer)
    {
      for(int y = 0, end = height >> 1; y < end; ++y)
      {
        for(int x = 0, end = width >> 1; x < end; ++x)
        {
          Color4 tl = rgbe(*(src + ((y << 1) + 0)*width + ((x << 1) + 0)));
          Color4 tr = rgbe(*(src + ((y << 1) + 0)*width + ((x << 1) + 1)));
          Color4 bl = rgbe(*(src + ((y << 1) + 1)*width + ((x << 1) + 0)));
          Color4 br = rgbe(*(src + ((y << 1) + 1)*width + ((x << 1) + 1)));

          *dst++ = rgbe((tl + tr + bl + br) / 4);
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
  uint32_t *src = (uint32_t*)bits;
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
  uint32_t *src = (uint32_t*)bits;
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
  uint32_t *src = (uint32_t*)bits;
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
  uint32_t *src = (uint32_t*)bits;
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
}
