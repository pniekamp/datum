//
// Datum - asset pack
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include <cstdint>
#include <functional>
#include <cstring>

#pragma pack(push, 1)

struct PackHeader
{
  uint8_t signature[8];
};

struct PackChunk
{
  uint32_t length;
  uint32_t type;
};

struct PackBlock
{
  uint32_t size;
  uint8_t data[4092];
};

struct PackAssetHeader
{
  uint32_t id;
};

struct PackTextHeader
{
  uint32_t length;
  uint64_t dataoffset;
};

struct PackTextPayload
{
// char string[length];
};

struct PackImageHeader
{
  uint32_t width;
  uint32_t height;
  uint32_t layers;
  uint32_t levels;
  float alignx;
  float aligny;
  uint32_t datasize;
  uint64_t dataoffset;
};

struct PackImagePayload
{
  enum { none = 0, bc3 = 3 };

  uint32_t compression;
// uint32_t image[levels][layers][height][width];
};

struct PackFontHeader
{
  uint32_t ascent;
  uint32_t descent;
  uint32_t leading;
  uint32_t glyphcount;
  uint64_t dataoffset;
};

struct PackFontPayload
{
  uint32_t glyphatlas;
// uint16_t x[glyphcount];
// uint16_t y[glyphcount];
// uint16_t width[glyphcount];
// uint16_t height[glyphcount];
// uint16_t offsetx[glyphcount];
// uint16_t offsety[glyphcount];
// uint8_t advance[glyphcount][glyphcount];
};

struct PackVertex
{
  float position[3];
  float texcoord[2];
  float normal[3];
  float tangent[4];
};

struct PackMeshHeader
{
  uint32_t vertexcount;
  uint32_t indexcount;
  float mincorder[3];
  float maxcorder[3];
  uint64_t dataoffset;
};

struct PackMeshPayload
{
// float verticies[vertexcount][sizeof(PackVertex)];
// uint32_t indicies[indexcount];
};

struct PackMaterialHeader
{
  uint64_t dataoffset;
};

struct PackMaterialPayload
{
  float albedocolor[3];
  uint32_t albedomap;

  float specularintensity[3];
  float specularexponent;

  uint32_t specularmap;

  uint32_t normalmap;
};

struct PackModelHeader
{
  uint32_t texturecount;
  uint32_t materialcount;
  uint32_t meshcount;
  uint32_t instancecount;
  uint64_t dataoffset;
};

struct PackModelPayload
{
  struct Texture
  {
    enum { defaulttexture, albedomap, normalmap, specularmap };

    uint32_t type;
    uint32_t texture;
  };

  struct Material
  {
    float albedocolor[3];
    uint32_t albedomap;

    float specularintensity[3];
    float specularexponent;

    uint32_t specularmap;

    uint32_t normalmap;
  };

  struct Mesh
  {
    uint32_t mesh;
  };

  struct Instance
  {
    uint32_t mesh;
    uint32_t material;
    float transform[8];
    uint32_t childcount;
  };

//  Texture textures[normaltexturecount];
//  Material materials[materialcount];
//  Mesh meshes[meshcount];
//  Instance instances[instancecount];
};

struct PackCalalogHeader
{
};

#pragma pack(pop)

constexpr uint32_t operator"" _packchunktype(const char* str, size_t len)
{
  return (str[3] << 24) | (str[2] << 16) | (str[1] << 8) | (str[0] << 0);
}

namespace std
{
  template<>
  struct hash<PackVertex>
  {
    size_t operator()(PackVertex const &vertex) const
    {
      return (hash<float>()(vertex.position[0]) << 0) ^ (hash<float>()(vertex.position[1]) << 1) ^ (hash<float>()(vertex.position[2]) << 2);
    }
  };
}

inline bool operator==(PackVertex const &lhs, PackVertex const &rhs)
{
  return memcmp(&lhs, &rhs, sizeof(PackVertex)) == 0;
}
