//
// Datum - glsl utils
//

#pragma once

#include <leap/pathstring.h>
#include <vector>
#include <cstdint>

enum class ShaderStage
{
  Vertex,
  Geometry,
  Fragment,
  Compute
};

std::string load_shader(leap::pathstring const &path, std::string const &defines = "");

///////////////////////// compile_shader ////////////////////////////////////
std::vector<uint8_t> compile_shader(std::string const &text, ShaderStage stage);
