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
    data = (void const *)((char const *)data + bytes);
  }

  write_chunk(fout, type, payload.size(), payload.data());
}


///////////////////////// write_catl_asset //////////////////////////////////
uint32_t write_catl_asset(ostream &fout, uint32_t id, uint32_t magic, uint32_t version, std::vector<std::tuple<uint32_t, std::string>> const &entries)
{
  size_t stringslength = 0;

  for(auto &entry : entries)
    stringslength += get<1>(entry).size() + 1;

  vector<uint8_t> payload;

  pack<uint32_t>(payload, entries.size());
  pack<uint32_t>(payload, stringslength);

  int i = 0;
  for(auto &entry : entries)
  {
    PackCatalogPayload::Entry ed;
    ed.id = get<0>(entry);
    ed.pathindex = i;
    ed.pathlength = get<1>(entry).size();

    pack<PackCatalogPayload::Entry>(payload, ed);

    i += get<1>(entry).size() + 1;
  }

  for(auto &entry : entries)
  {
    pack<char>(payload, get<1>(entry).c_str(), get<1>(entry).size() + 1);
  }

  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackCalalogHeader catl = { magic, version, (uint32_t)payload.size(), (uint64_t)fout.tellp() + sizeof(catl) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "CATL", sizeof(catl), &catl);

  write_chunk(fout, "DATA", pack_payload_size(catl), payload.data());

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_text_asset //////////////////////////////////
uint32_t write_text_asset(ostream &fout, uint32_t id, uint32_t length, void const *data)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackTextHeader text = { length, (uint64_t)fout.tellp() + sizeof(text) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "TEXT", sizeof(text), &text);

  write_chunk(fout, "DATA", pack_payload_size(text), data);

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

    case PackImageHeader::f32:
      datasize += image_datasize(width, height, layers, levels);
      break;

    default:
      assert(false);
  }

  PackImageHeader imag = { width, height, layers, levels, format, datasize, (uint64_t)fout.tellp() + sizeof(imag) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "IMAG", sizeof(imag), &imag);

  write_chunk(fout, "DATA", pack_payload_size(imag), bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_font_asset //////////////////////////////////
uint32_t write_font_asset(ostream &fout, uint32_t id, uint32_t ascent, uint32_t descent, uint32_t leading, uint32_t glyphcount, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackFontHeader font = { ascent, descent, leading, glyphcount, (uint64_t)fout.tellp() + sizeof(font) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "FONT", sizeof(font), &font);

  write_chunk(fout, "DATA", pack_payload_size(font), bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_font_asset //////////////////////////////////
uint32_t write_font_asset(ostream &fout, uint32_t id, uint32_t ascent, uint32_t descent, uint32_t leading, uint32_t glyphcount, uint32_t glyphatlas, vector<uint16_t> const &x, vector<uint16_t> const &y, vector<uint16_t> const &width, vector<uint16_t> const &height, vector<int16_t> const &offsetx, vector<int16_t> const &offsety, vector<uint8_t> const &advance)
{
  vector<uint8_t> payload;

  pack<uint32_t>(payload, glyphatlas);
  pack<uint16_t>(payload, x.data(), x.size());
  pack<uint16_t>(payload, y.data(), y.size());
  pack<uint16_t>(payload, width.data(), width.size());
  pack<uint16_t>(payload, height.data(), height.size());
  pack<uint16_t>(payload, offsetx.data(), offsetx.size());
  pack<uint16_t>(payload, offsety.data(), offsety.size());
  pack<uint8_t>(payload, advance.data(), advance.size());

  write_font_asset(fout, id, ascent, descent, leading, glyphcount, payload.data());

  return id + 1;
}


///////////////////////// write_mesh_asset //////////////////////////////////
uint32_t write_mesh_asset(ostream &fout, uint32_t id, uint32_t vertexcount, uint32_t indexcount, uint32_t bonecount, Bound3 const &bound, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  uint32_t datasize = 0;

  datasize += vertexcount*sizeof(PackVertex) + indexcount*sizeof(uint32_t);

  if (bonecount != 0)
  {
    datasize += vertexcount*sizeof(PackMeshPayload::Rig) + bonecount*sizeof(PackMeshPayload::Bone);
  }

  PackMeshHeader mesh = { vertexcount, indexcount, bonecount, bound.min.x, bound.min.y, bound.min.z, bound.max.x, bound.max.y, bound.max.z, datasize, (uint64_t)fout.tellp() + sizeof(mesh) + sizeof(PackChunk) + sizeof(uint32_t) };

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

  vector<uint8_t> payload;

  pack<PackVertex>(payload, vertices.data(), vertices.size());
  pack<uint32_t>(payload, indices.data(), indices.size());

  write_mesh_asset(fout, id, vertices.size(), indices.size(), 0, bound, payload.data());

  return id + 1;
}


///////////////////////// write_mesh_asset //////////////////////////////////
uint32_t write_mesh_asset(ostream &fout, uint32_t id, vector<PackVertex> const &vertices, vector<uint32_t> const &indices, vector<PackMeshPayload::Rig> const &rig, vector<PackMeshPayload::Bone> const &bones)
{
  Bound3 bound = bound_limits<Bound3>::min();

  for(auto &vertex : vertices)
  {
    bound = expand(bound, Vec3(vertex.position[0], vertex.position[1], vertex.position[2]));
  }

  vector<uint8_t> payload;

  pack<PackVertex>(payload, vertices.data(), vertices.size());
  pack<uint32_t>(payload, indices.data(), indices.size());
  pack<PackMeshPayload::Rig>(payload, rig.data(), rig.size());
  pack<PackMeshPayload::Bone>(payload, bones.data(), bones.size());

  write_mesh_asset(fout, id, vertices.size(), indices.size(), bones.size(), bound, payload.data());

  return id + 1;
}


///////////////////////// write_matl_asset //////////////////////////////////
uint32_t write_matl_asset(ostream &fout, uint32_t id, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackMaterialHeader matl = { (uint64_t)fout.tellp() + sizeof(matl) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "MATL", sizeof(matl), &matl);

  write_chunk(fout, "DATA", pack_payload_size(matl), bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_matl_asset //////////////////////////////////
uint32_t write_matl_asset(ostream &fout, uint32_t id, Color4 const &color, float metalness, float roughness, float reflectivity, float emissive, uint32_t albedomap, uint32_t surfacemap, uint32_t normalmap)
{
  PackMaterialPayload matl;

  matl.color[0] = color.r;
  matl.color[1] = color.g;
  matl.color[2] = color.b;
  matl.color[3] = color.a;
  matl.metalness = metalness;
  matl.roughness = roughness;
  matl.reflectivity = reflectivity;
  matl.emissive = emissive;
  matl.albedomap = albedomap;
  matl.surfacemap = surfacemap;
  matl.normalmap = normalmap;

  write_matl_asset(fout, id, &matl);

  return id + 1;
}


///////////////////////// write_anim_asset //////////////////////////////////
uint32_t write_anim_asset(ostream &fout, uint32_t id, float duration, uint32_t jointcount, uint32_t transformcount, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackAnimationHeader anim = { duration, jointcount, transformcount, (uint64_t)fout.tellp() + sizeof(anim) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "ANIM", sizeof(anim), &anim);

  write_chunk(fout, "DATA", pack_payload_size(anim), bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_anim_asset //////////////////////////////////
uint32_t write_anim_asset(ostream &fout, uint32_t id, float duration, vector<PackAnimationPayload::Joint> const &joints, vector<PackAnimationPayload::Transform> const &transforms)
{
  vector<uint8_t> payload(sizeof(PackAnimationPayload)); // Note: Empty Payload has one byte

  pack<PackAnimationPayload::Joint>(payload, joints.data(), joints.size());
  pack<PackAnimationPayload::Transform>(payload, transforms.data(), transforms.size());

  write_anim_asset(fout, id, duration, joints.size(), transforms.size(), payload.data());

  return id + 1;
}


///////////////////////// write_part_asset //////////////////////////////////
uint32_t write_part_asset(ostream &fout, uint32_t id, Bound3 const &bound, uint32_t maxparticles, uint32_t emittercount, uint32_t emitterssize, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackParticleSystemHeader part = { bound.min.x, bound.min.y, bound.min.z, bound.max.x, bound.max.y, bound.max.z, maxparticles, emittercount, emitterssize, (uint64_t)fout.tellp() + sizeof(part) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "PART", sizeof(part), &part);

  write_chunk(fout, "DATA", pack_payload_size(part), bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_part_asset //////////////////////////////////
uint32_t write_part_asset(ostream &fout, uint32_t id, Bound3 const &bound, uint32_t spritesheet, uint32_t maxparticles, uint32_t emittercount, vector<uint8_t> const &emitters)
{
  vector<uint8_t> payload;

  pack<uint32_t>(payload, spritesheet);
  pack<uint8_t>(payload, emitters.data(), emitters.size());

  write_part_asset(fout, id, bound, maxparticles, emittercount, emitters.size(), payload.data());

  return id + 1;
}


///////////////////////// write_modl_asset //////////////////////////////////
uint32_t write_modl_asset(ostream &fout, uint32_t id, uint32_t texturecount, uint32_t materialcount, uint32_t meshcount, uint32_t instancecount, void const *bits)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackModelHeader modl = { texturecount, materialcount, meshcount, instancecount, (uint64_t)fout.tellp() + sizeof(modl) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "MODL", sizeof(modl), &modl);

  write_chunk(fout, "DATA", pack_payload_size(modl), bits);

  write_chunk(fout, "AEND", 0, nullptr);

  return id + 1;
}


///////////////////////// write_modl_asset //////////////////////////////////
uint32_t write_modl_asset(ostream &fout, uint32_t id, vector<PackModelPayload::Texture> const &textures, vector<PackModelPayload::Material> const &materials, vector<PackModelPayload::Mesh> const &meshes, vector<PackModelPayload::Instance> const &instances)
{
  vector<uint8_t> payload(sizeof(PackModelPayload)); // Note: Empty Payload has one byte

  pack<PackModelPayload::Texture>(payload, textures.data(), textures.size());
  pack<PackModelPayload::Material>(payload, materials.data(), materials.size());
  pack<PackModelPayload::Mesh>(payload, meshes.data(), meshes.size());
  pack<PackModelPayload::Instance>(payload, instances.data(), instances.size());

  write_modl_asset(fout, id, textures.size(), materials.size(), meshes.size(), instances.size(), payload.data());

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
