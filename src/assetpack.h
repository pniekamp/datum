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
#include <vector>

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
  uint8_t data[16380];
};

struct PackAssetHeader
{
  uint32_t id;
};

struct PackCalalogHeader
{
  uint32_t magic;
  uint32_t version;
  uint32_t length;
  uint64_t dataoffset;
};

constexpr size_t pack_payload_size(PackCalalogHeader const &catl)
{
  return catl.length;
}

struct PackTextHeader
{
  uint32_t length;
  uint64_t dataoffset;
};

struct PackTextPayload
{
// char string[length];
};

constexpr size_t pack_payload_size(PackTextHeader const &text)
{
  return text.length;
}

struct PackImageHeader
{
  enum { rgba = 0, rgba_bc3 = 3, rgbe = 5 };

  uint32_t width;
  uint32_t height;
  uint32_t layers;
  uint32_t levels;
  uint32_t format;
  uint32_t datasize;
  uint64_t dataoffset;
};

struct PackImagePayload
{
// uint32_t image[levels][layers][height][width];
};

constexpr size_t pack_payload_size(PackImageHeader const &imag)
{
  return imag.datasize;
}

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
// int16_t offsetx[glyphcount];
// int16_t offsety[glyphcount];
// uint8_t advance[glyphcount][glyphcount];

  static auto xtable(void const *bits, int glyphcount) { return reinterpret_cast<uint16_t const *>((size_t)bits + sizeof(uint32_t)); }
  static auto ytable(void const *bits, int glyphcount) { return reinterpret_cast<uint16_t const *>((size_t)bits + sizeof(uint32_t) + glyphcount*sizeof(uint16_t)); }
  static auto widthtable(void const *bits, int glyphcount) { return reinterpret_cast<uint16_t const *>((size_t)bits + sizeof(uint32_t) + 2*glyphcount*sizeof(uint16_t)); }
  static auto heighttable(void const *bits, int glyphcount) { return reinterpret_cast<uint16_t const *>((size_t)bits + sizeof(uint32_t) + 3*glyphcount*sizeof(uint16_t)); }
  static auto offsetxtable(void const *bits, int glyphcount) { return reinterpret_cast<int16_t const *>((size_t)bits + sizeof(uint32_t) + 4*glyphcount*sizeof(uint16_t)); }
  static auto offsetytable(void const *bits, int glyphcount) { return reinterpret_cast<int16_t const *>((size_t)bits + sizeof(uint32_t) + 5*glyphcount*sizeof(uint16_t)); }
  static auto advancetable(void const *bits, int glyphcount) { return reinterpret_cast<uint8_t const *>((size_t)bits + sizeof(uint32_t) + 6*glyphcount*sizeof(uint16_t)); }
};

constexpr size_t pack_payload_size(PackFontHeader const &font)
{
  return sizeof(uint32_t) + 6*font.glyphcount*sizeof(uint16_t) + font.glyphcount*font.glyphcount*sizeof(uint8_t);
}

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
  uint32_t bonecount;
  float mincorner[3];
  float maxcorner[3];
  uint64_t dataoffset;
};

struct PackMeshPayload
{   
// PackVertex verticies[vertexcount];
// uint32_t indicies[indexcount];

  static auto vertextable(void const *bits, int vertexcount, int indexcount) { return reinterpret_cast<PackVertex const *>(bits); }
  static auto indextable(void const *bits, int vertexcount, int indexcount) { return reinterpret_cast<uint32_t const *>((size_t)bits + vertexcount*sizeof(PackVertex)); }

  struct Rig
  {
    uint32_t bone[4];
    float weight[4];
  };

  struct Bone
  {
    char name[32];
    float transform[8];
  };

  // if bonecount != 0
  //   Rig rig[vertexcount];
  //   Bone bones[bonecount];

  static auto rigtable(void const *bits, int vertexcount, int indexcount) { return reinterpret_cast<Rig const *>((size_t)bits + vertexcount*sizeof(PackVertex) + indexcount*sizeof(uint32_t)); }
  static auto bonetable(void const *bits, int vertexcount, int indexcount) { return reinterpret_cast<Bone const *>((size_t)bits + vertexcount*sizeof(PackVertex) + indexcount*sizeof(uint32_t) + vertexcount*sizeof(Rig)); }
};

constexpr size_t pack_payload_size(PackMeshHeader const &mesh)
{
  size_t size = mesh.vertexcount*sizeof(PackVertex) + mesh.indexcount*sizeof(uint32_t);

  if (mesh.bonecount != 0)
  {
    size += mesh.vertexcount*sizeof(PackMeshPayload::Rig) + mesh.bonecount*sizeof(PackMeshPayload::Bone);
  }

  return size;
}

struct PackMaterialHeader
{
  uint64_t dataoffset;
};

struct PackMaterialPayload
{
  float color[4];
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;

  uint32_t albedomap;
  uint32_t specularmap;
  uint32_t normalmap;
};

constexpr size_t pack_payload_size(PackMaterialHeader const &matl)
{
  return sizeof(PackMaterialPayload);
}

struct PackAnimationHeader
{
  float duration;
  uint32_t jointcount;
  uint32_t transformcount;
  uint64_t dataoffset;
};

struct PackAnimationPayload
{
  struct Joint
  {
    char name[32];
    uint32_t parent;

    uint32_t index;
    uint32_t count;
  };

  struct Transform
  {
    float time;
    float transform[8];
  };

  // Joint joints[jointcount];
  // Transform transforms[transformcount];

  static auto jointtable(void const *bits, int jointcount, int transformcount) { return reinterpret_cast<Joint const *>((size_t)bits + sizeof(PackAnimationPayload)); }
  static auto transformtable(void const *bits, int jointcount, int transformcount) { return reinterpret_cast<Transform const *>((size_t)bits + sizeof(PackAnimationPayload) + jointcount*sizeof(Joint)); }
};

constexpr size_t pack_payload_size(PackAnimationHeader const &anim)
{
  return sizeof(PackAnimationPayload) + anim.jointcount*sizeof(PackAnimationPayload::Joint) + anim.transformcount*sizeof(PackAnimationPayload::Transform);
}

struct PackParticleSystemHeader
{
  float minrange[3];
  float maxrange[3];
  uint32_t maxparticles;
  uint32_t emittercount;
  uint32_t emitterssize;
  uint64_t dataoffset;
};

struct PackParticleSystemPayload
{
  uint32_t spritesheet;
  // PackEmitter emitters[emittercount];

  static auto emitters(void const *bits) { return reinterpret_cast<uint8_t const *>((size_t)bits + sizeof(uint32_t)); }
};

void pack(std::vector<uint8_t> &bits, class ParticleEmitter const &emitter);
void unpack(class ParticleEmitter &emitter, void const *bits, size_t &cursor);

constexpr size_t pack_payload_size(PackParticleSystemHeader const &part)
{
  return sizeof(PackParticleSystemPayload) + part.emitterssize;
}


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
    enum { nullmap, albedomap, specularmap, normalmap };

    uint32_t type;
    uint32_t texture;
  };

  struct Material
  {
    float color[4];
    float metalness;
    float roughness;
    float reflectivity;
    float emissive;

    uint32_t albedomap;
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

//  Texture textures[texturecount];
//  Material materials[materialcount];
//  Mesh meshes[meshcount];
//  Instance instances[instancecount];

  static auto texturetable(void const *bits, int texturecount, int materialcount, int meshcount, int instancecount) { return reinterpret_cast<Texture const *>(bits); }
  static auto materialtable(void const *bits, int texturecount, int materialcount, int meshcount, int instancecount) { return reinterpret_cast<Material const *>((size_t)bits + texturecount*sizeof(Texture)); }
  static auto meshtable(void const *bits, int texturecount, int materialcount, int meshcount, int instancecount) { return reinterpret_cast<Mesh const *>((size_t)bits + texturecount*sizeof(Texture) + materialcount*sizeof(Material)); }
  static auto instancetable(void const *bits, int texturecount, int materialcount, int meshcount, int instancecount) { return reinterpret_cast<Instance const *>((size_t)bits + texturecount*sizeof(Texture) + materialcount*sizeof(Material) + meshcount*sizeof(Mesh)); }
};

constexpr size_t pack_payload_size(PackModelHeader const &modl)
{
  return modl.texturecount*sizeof(PackModelPayload::Texture) + modl.materialcount*sizeof(PackModelPayload::Material) + modl.meshcount*sizeof(PackModelPayload::Mesh) + modl.instancecount*sizeof(PackModelPayload::Instance);
}

#pragma pack(pop)

constexpr uint32_t operator "" _packchunktype(const char* str, size_t len)
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

inline bool operator ==(PackVertex const &lhs, PackVertex const &rhs)
{
  return memcmp(&lhs, &rhs, sizeof(PackVertex)) == 0;
}
