//
// Datum - glsl utils
//

#include "glslang.h"
#include <leap/pathstring.h>
#include <iostream>
#include <fstream>

using namespace std;
using namespace leap;

///////////////////////// load_shader ///////////////////////////////////////
string load_shader(pathstring const &path, string const &defines)
{
  ifstream fin(path);

  if (!fin)
    throw runtime_error("Error opening - " + path.name());

  int line = 1;

  string shader = "#line " + to_string(line) + "\"" + path.name() + "\"\n";

  string buffer;

  while (getline(fin, buffer))
  {
    ++line;

    if (buffer.substr(0, 8) == "#version")
    {
      shader = "";
      buffer += "\n#extension GL_GOOGLE_cpp_style_line_directive : enable\n";
      buffer += "\n" + defines + "\n";
      buffer += "\n#line " + to_string(line) + "\"" + path.name() + "\"";
    }

    if (buffer.substr(0, 8) == "#include")
    {
      buffer = load_shader(pathstring(path.base(), string(buffer.begin() + buffer.find_first_of('"') + 1, buffer.begin() + buffer.find_last_of('"'))));

      buffer += "\n#line " + to_string(line) + "\"" + path.name() + "\"";
    }

    shader += buffer + '\n';
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

  ofstream(tmpname) << text << '\n';

#ifdef _WIN32
//    if (system(string("glslangValidator.exe -V -o tmp.spv " + tmpname).c_str()) != 0)
//      throw runtime_error("Error Executing glslangValidator");

//    if (system(string("spirv-remap --do-everything --input tmp.spv -o .").c_str()) != 0)
//      throw runtime_error("Error Executing glslangValidator");

//  if (system(string("glslc.exe -flimit=\"MaxDrawBuffers 4\" -o tmp.spv " + tmpname).c_str()) != 0)
//    throw runtime_error("Error Executing glslc");

  string opts;
  opts += "--remove-duplicates ";
  opts += "--merge-return ";
  opts += "--inline-entry-points-exhaustive ";
  opts += "--eliminate-dead-functions ";
  opts += "--scalar-replacement ";
  opts += "--convert-local-access-chains ";
  opts += "--eliminate-local-single-block ";
  opts += "--eliminate-local-single-store ";
  opts += "--ccp ";
  opts += "--redundancy-elimination ";
  opts += "--eliminate-insert-extract ";
  opts += "--eliminate-dead-inserts ";
  opts += "--eliminate-dead-branches ";
  opts += "--strip-debug";

  if (system(string("glslc.exe -flimit=\"MaxDrawBuffers 4\" -o tmp " + tmpname + " && spirv-opt " + opts + " -o tmp.spv tmp").c_str()) != 0)
    throw runtime_error("Error Executing glslc");

  system("spirv-val tmp.spv");

#else
  if (system(string("glslangValidator -V -o tmp.spv " + tmpname).c_str()) != 0)
    throw runtime_error("Error Executing glslangValidator");
#endif

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
  remove("tmp");

  return spirv;
}
