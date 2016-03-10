//
// Datum - asset packer
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "assetpacker.h"
#include "leap/lz4.h"

using namespace std;
using namespace lml;
using namespace leap::crypto;


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


///////////////////////// write_imag_asset //////////////////////////////////
uint32_t write_imag_asset(ostream &fout, uint32_t id, uint32_t width, uint32_t height, uint32_t layers, void const *bits, float alignx, float aligny)
{
  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackImageHeader ihdr = { width, height, layers, alignx, aligny, (size_t)fout.tellp() + sizeof(ihdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "IMAG", sizeof(ihdr), &ihdr);

  write_chunk(fout, "DATA", width * height * layers * sizeof(uint32_t), bits);

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
