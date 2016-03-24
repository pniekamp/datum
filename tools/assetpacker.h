//
// Datum - asset packer
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "datum/assetpack.h"
#include "datum/math.h"
#include <iostream>
#include <vector>


void write_header(std::ostream &fout);
void write_chunk(std::ostream &fout, const char type[4], uint32_t length, void const *data);
void write_compressed_chunk(std::ostream &fout, const char type[4], uint32_t length, void const *data);

uint32_t write_catl_asset(std::ostream &fout, uint32_t id);
uint32_t write_text_asset(std::ostream &fout, uint32_t id, std::string const &str);
uint32_t write_text_asset(std::ostream &fout, uint32_t id, std::vector<uint8_t> const &str);
uint32_t write_imag_asset(std::ostream &fout, uint32_t id, uint32_t width, uint32_t height, uint32_t layers, void const *bits, float alignx, float aligny);
uint32_t write_mesh_asset(std::ostream &fout, uint32_t id, std::vector<PackVertex> const &vertices, std::vector<uint32_t> const &indices);
uint32_t write_matl_asset(std::ostream &fout, uint32_t id, lml::Color3 albedocolor, uint32_t albedomap, lml::Color3 specularintensity, float specularexponent, uint32_t specularmap, uint32_t normalmap);

