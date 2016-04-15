//
// Datum - glsl utils
//

#pragma once

#include <vector>
#include <string>

enum class ShaderStage
{
  Vertex,
  Geometry,
  Fragment,
  Compute
};

std::string load_shader(std::string const &path);
std::vector<uint8_t> compile_shader(std::string const &text, ShaderStage stage);

