//
// Datum - asset packer
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "datum/assetpack.h"
#include "datum/math.h"
#include <iostream>
#include <vector>

//
// Pack Writers
//

void write_header(std::ostream &fout);
void write_chunk(std::ostream &fout, const char type[4], uint32_t length, void const *data);
void write_compressed_chunk(std::ostream &fout, const char type[4], uint32_t length, void const *data);

uint32_t write_catl_asset(std::ostream &fout, uint32_t id);
uint32_t write_text_asset(std::ostream &fout, uint32_t id, uint32_t length, void const *data);
uint32_t write_imag_asset(std::ostream &fout, uint32_t id, uint32_t width, uint32_t height, uint32_t layers, uint32_t levels, uint32_t format, void const *bits);
uint32_t write_font_asset(std::ostream &fout, uint32_t id, uint32_t ascent, uint32_t descent, uint32_t leading, uint32_t glyphcount, void const *bits);
uint32_t write_font_asset(std::ostream &fout, uint32_t id, uint32_t ascent, uint32_t descent, uint32_t leading, uint32_t glyphcount, uint32_t glyphatlas, std::vector<uint16_t> const &x, std::vector<uint16_t> const &y, std::vector<uint16_t> const &width, std::vector<uint16_t> const &height, std::vector<uint16_t> const &offsetx, std::vector<uint16_t> const &offsety, std::vector<uint8_t> const &advance);
uint32_t write_mesh_asset(std::ostream &fout, uint32_t id, uint32_t vertexcount, uint32_t indexcount, lml::Bound3 const &bound, void const *bits);
uint32_t write_mesh_asset(std::ostream &fout, uint32_t id, std::vector<PackVertex> const &vertices, std::vector<uint32_t> const &indices);
uint32_t write_matl_asset(std::ostream &fout, uint32_t id, void const *bits);
uint32_t write_matl_asset(std::ostream &fout, uint32_t id, lml::Color3 const &color, float metalness, float roughness, float reflectivity, float emissive, uint32_t albedomap, uint32_t specularmap, uint32_t normalmap);
uint32_t write_modl_asset(std::ostream &fout, uint32_t id, uint32_t texturecount, uint32_t materialcount, uint32_t meshcount, uint32_t instancecount, void const *bits);
uint32_t write_modl_asset(std::ostream &fout, uint32_t id, std::vector<PackModelPayload::Texture> const &textures, std::vector<PackModelPayload::Material> const &materials, std::vector<PackModelPayload::Mesh> const &meshes, std::vector<PackModelPayload::Instance> const &instances);


//
// Image Utilities
//

int image_maxlevels(int width, int height);

size_t image_datasize(int width, int height, int layers, int levels);
size_t image_datasize_bc3(int width, int height, int layers, int levels);

void image_buildmips_rgb(int width, int height, int layers, int levels, void *bits);
void image_buildmips_rgbe(int width, int height, int layers, int levels, void *bits);
void image_buildmips_srgb(int width, int height, int layers, int levels, void *bits);
void image_buildmips_srgb_a(float alpharef, int width, int height, int layers, int levels, void *bits);
void image_premultiply_srgb(int width, int height, int layers, int levels, void *bits);

void image_compress_bc3(int width, int height, int layers, int levels, void *bits);
