//
// Datum - glsl utils
//

#include "glslang.h"
#include <iostream>
#include <fstream>

using namespace std;


///////////////////////// load_shader ///////////////////////////////////////
string load_shader(string const &path)
{
  string shader;

  ifstream fin(path);
  if (!fin)
    throw runtime_error("Error opening - " + path);

  string base = path.substr(0, path.find_last_of("/\\"));

  string buffer;

  while (getline(fin, buffer))
  {
    if (buffer.empty() || buffer.substr(0, 2) == "//")
      continue;

    if (buffer.substr(0, 8) == "#include")
    {
      buffer = load_shader(base + "/" + string(buffer.begin() + buffer.find_first_of("\"") + 1, buffer.begin() + buffer.find_last_of("\"")));
    }

    shader += buffer + "\n";
  }

  return shader;
}


///////////////////////// compile_shader ////////////////////////////////////
vector<uint8_t> compile_shader(string const &text, ShaderStage stage)
{
  string tmpname;

  switch(stage)
  {
    case ShaderStage::Vertex:
      tmpname = "tmp.vert";
      break;

    case ShaderStage::Geometry:
      tmpname = "tmp.geom";
      break;

    case ShaderStage::Fragment:
      tmpname = "tmp.frag";
      break;

    case ShaderStage::Compute:
      tmpname = "tmp.comp";
      break;
  }

  ofstream(tmpname) << text << "\n";

  if (system(string("glslangValidator.exe -V -o tmp.spv " + tmpname).c_str()) != 0)
    throw runtime_error("Error Executing glslangValidator");

//  if (system(string("spirv-remap --do-everything --input tmp.spv -o .").c_str()) != 0)
//    throw runtime_error("Error Executing glslangValidator");

  remove(tmpname.c_str());

  vector<uint8_t> spirv;

  ifstream fin("tmp.spv", ios::binary);
  if (!fin)
    throw runtime_error("Error compiling shader");

  fin.seekg(0, ios::end);
  spirv.resize(fin.tellg());
  fin.seekg(0, ios::beg);
  fin.read((char*)spirv.data(), spirv.size());
  fin.close();

  remove("tmp.spv");

  return spirv;
}
