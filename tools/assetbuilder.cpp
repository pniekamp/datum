//
// Datum - asset builder
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include <QGuiApplication>
#include <QImage>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <fstream>
#include <unordered_map>
#include <cassert>
#include "glslang.h"
#include "assetpacker.h"
#include "atlaspacker.h"
#include <leap.h>

using namespace std;
using namespace lml;
using namespace leap;

namespace
{
  vector<string> seperate(string const &str, const char *delimiters = " \t\r\n")
  {
    vector<string> result;

    size_t i = 0;
    size_t j = 0;

    while (j != string::npos)
    {
      j = str.find_first_of(delimiters, i);

      result.push_back(str.substr(i, j-i));

      i = j + 1;
    }

    return result;
  }


  void calculatetangents(vector<PackVertex> &vertices, vector<uint32_t> &indices)
  {
    vector<Vec3> tan1(vertices.size(), Vec3(0.0f, 0.0f, 0.0f));
    vector<Vec3> tan2(vertices.size(), Vec3(0.0f, 0.0f, 0.0f));

    for(size_t i = 0; i < indices.size(); i += 3)
    {
      auto &v1 = vertices[indices[i+0]];
      auto &v2 = vertices[indices[i+1]];
      auto &v3 = vertices[indices[i+2]];

      auto x1 = v2.position[0] - v1.position[0];
      auto x2 = v3.position[0] - v1.position[0];
      auto y1 = v2.position[1] - v1.position[1];
      auto y2 = v3.position[1] - v1.position[1];
      auto z1 = v2.position[2] - v1.position[2];
      auto z2 = v3.position[2] - v1.position[2];

      auto s1 = v2.texcoord[0] - v1.texcoord[0];
      auto s2 = v3.texcoord[0] - v1.texcoord[0];
      auto t1 = v2.texcoord[1] - v1.texcoord[1];
      auto t2 = v3.texcoord[1] - v1.texcoord[1];

      auto r = s1 * t2 - s2 * t1;

      if (r != 0)
      {
        auto sdir = Vec3(t2 * x1 - t1 * x2, t2 * y1 - t1 * y2, t2 * z1 - t1 * z2)/r;
        auto tdir = Vec3(s1 * x2 - s2 * x1, s1 * y2 - s2 * y1, s1 * z2 - s2 * z1)/r;

        auto uvarea = area(Vec2(v1.texcoord[0], v1.texcoord[1]), Vec2(v2.texcoord[0], v2.texcoord[1]), Vec2(v3.texcoord[0], v3.texcoord[1]));

        tan1[indices[i+0]] += sdir * uvarea;
        tan1[indices[i+1]] += sdir * uvarea;
        tan1[indices[i+2]] += sdir * uvarea;

        tan2[indices[i+0]] += tdir * uvarea;
        tan2[indices[i+1]] += tdir * uvarea;
        tan2[indices[i+2]] += tdir * uvarea;
      }
    }

    for(size_t i = 0; i < vertices.size(); ++i)
    {
      auto normal = Vec3(vertices[i].normal[0], vertices[i].normal[1], vertices[i].normal[2]);

      if (normsqr(tan1[i]) == 0)
        tan1[i] = cross(normal, Vec3(vertices[i].normal[1], -vertices[i].normal[2], vertices[i].normal[0]));

      if (normsqr(tan2[i]) == 0)
        tan2[i] = cross(normal, tan1[i]);

      if (normsqr(tan1[i] - normal * dot(normal, tan1[i])) == 0)
        tan1[i] = cross(normal, tan2[i]);

      auto tangent = normalise(tan1[i] - normal * dot(normal, tan1[i]));

      vertices[i].tangent[0] = tangent.x;
      vertices[i].tangent[1] = tangent.y;
      vertices[i].tangent[2] = tangent.z;
      vertices[i].tangent[3] = (dot(cross(normal, tan1[i]), tan2[i]) < 0.0f) ? -1.0f : 1.0f;
    }
  }

}


uint32_t write_catalog_asset(ostream &fout, uint32_t id)
{
  write_catl_asset(fout, id);

  return id + 1;
}


uint32_t write_shader_asset(ostream &fout, uint32_t id, string const &path)
{
  auto text = load_shader(path);

  ShaderStage stage;

  if (path.substr(path.find_last_of('.')) == ".vert")
    stage = ShaderStage::Vertex;
  else if (path.substr(path.find_last_of('.')) == ".geom")
    stage = ShaderStage::Geometry;
  else if (path.substr(path.find_last_of('.')) == ".frag")
    stage = ShaderStage::Fragment;
  else if (path.substr(path.find_last_of('.')) == ".comp")
    stage = ShaderStage::Compute;
  else
    throw runtime_error("Unknown shader stage");

  auto spirv = compile_shader(text, stage);

  write_text_asset(fout, id, spirv);

  cout << "  " << path << endl;

  return id + 1;
}


uint32_t write_image_asset(ostream &fout, uint32_t id, string const &path, float alignx = 0.5f, float aligny = 0.5f)
{
  QImage image(path.c_str());

  if (image.isNull())
    throw runtime_error("Failed to load image - " + path);

  image = image.convertToFormat(QImage::Format_ARGB32);

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = 1;

  vector<char> payload(sizeof(PackImagePayload) + image_datasize(width, height, layers, levels));

  memcpy(payload.data() + sizeof(PackImagePayload), image.bits(), image.byteCount());

  write_imag_asset(fout, id, width, height, layers, levels, payload.data(), alignx, aligny);

  cout << "  " << path << endl;

  return id + 1;
}

uint32_t write_sprite_asset(ostream &fout, uint32_t id, vector<QImage> const &images, float alignx = 0.5f, float aligny = 0.5f)
{
  int width = images.front().width();
  int height = images.front().height();
  int layers = images.size();
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(sizeof(PackImagePayload) + image_datasize(width, height, layers, levels));

  char *dst = payload.data() + sizeof(PackImagePayload);
  for(size_t i = 0; i < images.size(); i++)
  {
    QImage image = images[i].convertToFormat(QImage::Format_ARGB32);

    if (image.width() != width || image.height() != height)
      throw runtime_error("Layers with differing dimensions");

    memcpy(dst, image.bits(), image.byteCount());

    dst += image.byteCount();
  }

  image_premultiply_srgb(width, height, layers, levels, payload.data());

  image_buildmips_srgb(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, payload.data(), alignx, aligny);

  return id + 1;
}


uint32_t write_sprite_asset(ostream &fout, uint32_t id, std::vector<string> const &paths, float alignx = 0.5f, float aligny = 0.5f)
{
  vector<QImage> layers;

  for(auto &path : paths)
  {
    QImage image(path.c_str());

    if (image.isNull())
      throw runtime_error("Failed to load image - " + path);

    layers.push_back(image);

    cout << "  " << path << endl;
  }

  write_sprite_asset(fout, id, layers, alignx, aligny);

  return id + 1;
}


uint32_t write_sprite_asset(ostream &fout, uint32_t id, string const &path, float alignx = 0.5f, float aligny = 0.5f)
{
  write_sprite_asset(fout, id, vector<string>{ path }, alignx, aligny);

  return id + 1;
}


uint32_t write_sprite_asset(ostream &fout, uint32_t id, string const &path, int count, float alignx = 0.5f, float aligny = 0.5f)
{
  QImage sheet(path.c_str());

  if (sheet.isNull())
    throw runtime_error("Failed to load image - " + path);

  vector<QImage> layers;

  int width = sheet.width() / count;
  int height = sheet.height();

  for(int i = 0; i < sheet.width(); i += width)
  {
    layers.push_back(sheet.copy(i, 0, width, height));
  }

  write_sprite_asset(fout, id, layers, alignx, aligny);

  cout << "  " << path << endl;

  return id + 1;
}


uint32_t write_albedomap_asset(ostream &fout, uint32_t id, string const &path)
{
  QImage image(path.c_str());

  if (image.isNull())
    throw runtime_error("Failed to load image - " + path);

  image = image.convertToFormat(QImage::Format_ARGB32).mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(sizeof(PackImagePayload) + image_datasize(width, height, layers, levels));

  memcpy(payload.data() + sizeof(PackImagePayload), image.bits(), image.byteCount());

  image_buildmips_srgb_a(0.5, width, height, layers, levels, payload.data());

  image_compress_bc3(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, payload.data(), 0.0f, 0.0f);

  cout << "  " << path << endl;

  return id + 1;
}


uint32_t write_specularmap_asset(ostream &fout, uint32_t id, string const &path)
{
  QImage image(path.c_str());

  if (image.isNull())
    throw runtime_error("Failed to load image - " + path);

  image = image.convertToFormat(QImage::Format_ARGB32).mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(sizeof(PackImagePayload) + image_datasize(width, height, layers, levels));

  memcpy(payload.data() + sizeof(PackImagePayload), image.bits(), image.byteCount());

  image_buildmips_srgb(width, height, layers, levels, payload.data());

  image_compress_bc3(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, payload.data(), 0.0f, 0.0f);

  cout << "  " << path << endl;

  return id + 1;
}


uint32_t write_normalmap_asset(ostream &fout, uint32_t id, string const &path)
{
  QImage image(path.c_str());

  if (image.isNull())
    throw runtime_error("Failed to load image - " + path);

  image = image.convertToFormat(QImage::Format_ARGB32).mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(sizeof(PackImagePayload) + image_datasize(width, height, layers, levels));

  memcpy(payload.data() + sizeof(PackImagePayload), image.bits(), image.byteCount());

  image_buildmips_rgb(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, payload.data(), 0.0f, 0.0f);

  cout << "  " << path << endl;

  return id + 1;
}


uint32_t write_mesh_asset(ostream &fout, uint32_t id, string const &path, float scale = 1.0f)
{
  vector<Vec3> points;
  vector<Vec3> normals;
  vector<Vec2> texcoords;

  vector<PackVertex> vertices;
  vector<uint32_t> indices;
  unordered_map<PackVertex, uint32_t> vertexmap;

  ifstream fin(path);

  if (!fin)
    throw runtime_error("unable to read obj file - " + path);

  string buffer;

  while (getline(fin, buffer))
  {
    buffer = trim(buffer);

    // skip comments
    if (buffer.empty() || buffer[0] == '#' || buffer[0] == '/')
      continue;

    auto fields = split(buffer);

    if (fields[0] == "v")
    {
      points.push_back({ ato<float>(fields[1]), ato<float>(fields[2]), ato<float>(fields[3]) });
    }

    if (fields[0] == "vn")
    {
      normals.push_back({ ato<float>(fields[1]), ato<float>(fields[2]), ato<float>(fields[3]) });
    }

    if (fields[0] == "vt")
    {
      texcoords.push_back({ ato<float>(fields[1]), ato<float>(fields[2]) });
    }

    if (fields[0] == "f")
    {
      vector<string> face[] = { seperate(fields[1], "/"), seperate(fields[2], "/"), seperate(fields[3], "/") };

      for(auto &v : face)
      {
        PackVertex vertex = {};

        if (v.size() > 0 && ato<int>(v[0]) != 0)
          memcpy(vertex.position, &points[ato<int>(v[0]) - 1], sizeof(vertex.position));

        if (v.size() > 1 && ato<int>(v[1]) != 0)
          memcpy(vertex.texcoord, &texcoords[ato<int>(v[1]) - 1], sizeof(vertex.texcoord));

        if (v.size() > 2 && ato<int>(v[2]) != 0)
          memcpy(vertex.normal, &normals[ato<int>(v[2]) - 1], sizeof(vertex.normal));

        if (vertexmap.find(vertex) == vertexmap.end())
        {
          vertices.push_back(vertex);

          vertexmap[vertex] = vertices.size() - 1;
        }

        indices.push_back(vertexmap[vertex]);
      }
    }
  }

  for(auto &vertex : vertices)
  {
    vertex.position[0] *= scale;
    vertex.position[1] *= scale;
    vertex.position[2] *= scale;
  }

  calculatetangents(vertices, indices);

  write_mesh_asset(fout, id, vertices, indices);

  cout << "  " << path << endl;

  return id + 1;
}


uint32_t write_material_asset(ostream &fout, uint32_t id, Color3 albedocolor, string albedomap, Color3 specularintensity, float specularexponent, string specularmap, string normalmap)
{
  int mapid = 0;
  uint32_t albedomapid = (albedomap != "") ? ++mapid : 0;
  uint32_t specularmapid = (specularmap != "") ? ++mapid : 0;
  uint32_t normalmapid = (normalmap != "") ? ++mapid : 0;

  write_matl_asset(fout, id, albedocolor, albedomapid, specularintensity, specularexponent, specularmapid, normalmapid);

  if (albedomapid)
    write_albedomap_asset(fout, id + albedomapid, albedomap);

  if (specularmapid)
    write_specularmap_asset(fout, id + specularmapid, specularmap);

  if (normalmapid)
    write_normalmap_asset(fout, id + normalmapid, normalmap);

  return id + 1 + mapid;
}


uint32_t write_font_asset(ostream &fout, uint32_t id, string const &fontname, int size, int weight)
{
  QFont font(fontname.c_str(), size, weight);

  QFontMetrics tm(font);

  PackAssetHeader aset = { id };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  int count = 127;

  size_t datasize = sizeof(PackFontPayload) + 6*count*sizeof(uint16_t) + count*count*sizeof(uint8_t);

  PackFontHeader fhdr = { (uint32_t)tm.ascent(), (uint32_t)tm.descent(), (uint32_t)tm.leading(), (uint32_t)count, (size_t)fout.tellp() + sizeof(fhdr) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "FONT", sizeof(fhdr), &fhdr);

  vector<char> payload(datasize);

  reinterpret_cast<PackFontPayload*>(payload.data())->glyphatlas = 1;

  auto xtable = reinterpret_cast<uint16_t*>(payload.data() + sizeof(PackFontPayload) + 0 * count * sizeof(uint16_t));
  auto ytable = reinterpret_cast<uint16_t*>(payload.data() + sizeof(PackFontPayload) + 1 * count * sizeof(uint16_t));
  auto widthtable = reinterpret_cast<uint16_t*>(payload.data() + sizeof(PackFontPayload) + 2 * count * sizeof(uint16_t));
  auto heighttable = reinterpret_cast<uint16_t*>(payload.data() + sizeof(PackFontPayload) + 3 * count * sizeof(uint16_t));
  auto offsetxtable = reinterpret_cast<uint16_t*>(payload.data() + sizeof(PackFontPayload) + 4 * count * sizeof(uint16_t));
  auto offsetytable = reinterpret_cast<uint16_t*>(payload.data() + sizeof(PackFontPayload) + 5 * count * sizeof(uint16_t));
  auto advancetable = reinterpret_cast<uint8_t*>(payload.data() + sizeof(PackFontPayload) + 6 * count * sizeof(uint16_t));

  AtlasPacker packer(512, 256);

  for(int codepoint = 33; codepoint < count; ++codepoint)
  {
    auto position = packer.insert(codepoint, tm.width(QChar(codepoint))+12, tm.height()+2);

    assert(position);

    xtable[codepoint] = position->x;
    ytable[codepoint] = position->y;
    widthtable[codepoint] = position->width;
    heighttable[codepoint] = position->height;
    offsetxtable[codepoint] = 1;
    offsetytable[codepoint] = 1 + tm.ascent();
  }

  for(int codepoint = 0; codepoint < count; ++codepoint)
  {
    advancetable[codepoint] = 0;

    for(int othercodepoint = 1; othercodepoint < count; ++othercodepoint)
    {
      advancetable[othercodepoint * count + codepoint] = tm.width(QString(QChar(othercodepoint)) + QString(QChar(codepoint))) - tm.width(QChar(codepoint));
    }
  }

  write_chunk(fout, "DATA", payload.size(), payload.data());

  write_chunk(fout, "AEND", 0, nullptr);

  QImage atlas(packer.width, packer.height, QImage::Format_ARGB32);

  atlas.fill(0x00000000);

  for(int codepoint = 33; codepoint < count; ++codepoint)
  {
    auto position = packer.find(codepoint);

    QPainter painter(&atlas);

    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(position->x + 1, position->y + 1, position->width - 2, position->height - 2), QString(QChar(codepoint)));
  }

  write_sprite_asset(fout, id + 1, { atlas }, 0.0f, 0.0f);

  cout << "  " << fontname << endl;

  return id + 1 + count;
}


void write_material(string const &output, Color3 albedocolor, string albedomap, Color3 specularintensity, float specularexponent, string specularmap, string normalmap)
{
  ofstream fout(output, ios::binary | ios::trunc);

  write_header(fout);

  write_material_asset(fout, 0, albedocolor, albedomap, specularintensity, specularexponent, specularmap, normalmap);

  write_chunk(fout, "HEND", 0, nullptr);

  fout.close();
}


void write_mesh(string const &output, string const &filename, float scale = 1.0f)
{
  ofstream fout(output, ios::binary | ios::trunc);

  write_header(fout);

  write_mesh_asset(fout, 0, filename, scale);

  write_chunk(fout, "HEND", 0, nullptr);

  fout.close();
}


void write_core()
{
  ofstream fout("core.pack", ios::binary | ios::trunc);

  write_header(fout);

  write_catalog_asset(fout, CoreAsset::catalog);

  write_image_asset(fout, CoreAsset::white_diffuse, "../../data/white.png");
  write_image_asset(fout, CoreAsset::nominal_normal, "../../data/normal.png");

  write_mesh_asset(fout, CoreAsset::unit_quad, { { 0.0, 1.0, 0.0, 0.0, 1.0 }, { 0.0, 0.0, 0.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0, 1.0, 1.0 }, { 1.0, 0.0, 0.0, 1.0, 0.0 } }, { 0, 1, 2, 2, 1, 3 });
  write_mesh_asset(fout, CoreAsset::unit_cube, { { 0.0, 0.0, 1.0 }, { 1.0, 0.0, 1.0 }, { 1.0, 1.0, 1.0 }, { 0.0, 1.0, 1.0 }, { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0 }, { 0.0, 1.0, 0.0 } }, { 0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4, 3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4 });
  write_mesh_asset(fout, CoreAsset::homo_cube, { { -1.0, -1.0, 1.0 }, { 1.0, -1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { -1.0, 1.0, 1.0 }, { -1.0, -1.0, -1.0 }, { 1.0, -1.0, -1.0 }, { 1.0, 1.0, -1.0 }, { -1.0, 1.0, -1.0 } }, { 0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4, 3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4 });
  write_mesh_asset(fout, CoreAsset::unit_cone, { { 0.0, 0.0, 0.0 }, { 0.0, 1.0, -1.0 }, { -0.5f, 0.866, -1.0 }, { -0.866, 0.5, -1.0 }, { -1.0, 0.0, -1.0 }, { -0.866, -0.5, -1.0 }, { -0.5, -0.866, -1.0 }, { 0.0, -1.0, -1.0 }, { 0.5, -0.866, -1.0 }, { 0.866, -0.5, -1.0 }, { 1.0, 0.0, -1.0 }, { 0.866, 0.5, -1.0 }, { 0.5f, 0.866, -1.0 } }, { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 6, 0, 6, 7, 0, 7, 8, 0, 8, 9, 0, 9, 10, 0, 10, 11, 0, 11, 12, 0, 12, 1, 10, 6, 2, 10, 8, 6, 10, 9, 8, 8, 7, 6, 6, 4, 2, 6, 5, 4, 4, 3, 2, 2, 12, 10, 2, 1, 12, 12, 11, 10 });
  write_mesh_asset(fout, CoreAsset::unit_sphere, { { 0, 1, 0 }, { 0, -1, 0 }, { -0.707, 0, 0.707 }, { 0.707, 0, 0.707 }, { 0.707, 0, -0.707 }, { -0.707, 0, -0.707 }, { 0, 0, 1 }, { 0.499962, 0.70716, 0.499962 }, { -0.499962, 0.70716, 0.499962 }, { 1, 0, 0 }, { 0.499962, 0.70716, -0.499962 }, { 0.499962, 0.70716, 0.499962 }, { 0, 0, -1 }, { -0.499962, 0.70716, -0.499962 }, { 0.499962, 0.70716, -0.499962 }, { -1, 0, 0 }, { -0.499962, 0.70716, 0.499962 }, { -0.499962, 0.70716, -0.499962 }, { -0.499962, -0.70716, 0.499962 }, { 0.499962, -0.70716, 0.499962 }, { 0, 0, 1 }, { 0.499962, -0.70716, 0.499962 }, { 0.499962, -0.70716, -0.499962 }, { 1, 0, 0 }, { 0.499962, -0.70716, -0.499962 }, { -0.499962, -0.70716, -0.499962 }, { 0, 0, -1 }, { -0.499962, -0.70716, -0.499962 }, { -0.499962, -0.70716, 0.499962 }, { -1, 0, 0 }, { 0.288657, 0.408284, 0.866014 }, { 0, 0.816538, 0.577292 }, { -0.288657, 0.408284, 0.866014 }, { 0.866014, 0.408284, -0.288657 }, { 0.577292, 0.816538, 0 }, { 0.866014, 0.408284, 0.288657 }, { -0.288657, 0.408284, -0.866014 }, { 0, 0.816538, -0.577292 }, { 0.288657, 0.408284, -0.866014 }, { -0.866014, 0.408284, 0.288657 }, { -0.577292, 0.816538, 0 }, { -0.866014, 0.408284, -0.288657 }, { 0, -0.816538, 0.577292 }, { 0.288657, -0.408284, 0.866014 }, { -0.288657, -0.408284, 0.866014 }, { 0.577292, -0.816538, 0 }, { 0.866014, -0.408284, -0.288657 }, { 0.866014, -0.408284, 0.288657 }, { 0, -0.816538, -0.577292 }, { -0.288657, -0.408284, -0.866014 }, { 0.288657, -0.408284, -0.866014 }, { -0.577292, -0.816538, 0 }, { -0.866014, -0.408284, 0.288657 }, { -0.866014, -0.408284, -0.288657 }, { -0.382655, 0, 0.923891 }, { -0.288657, 0.408284, 0.866014 }, { -0.653263, 0.382747, 0.653263 }, { 0.382655, 0, 0.923891 }, { 0.653263, 0.382747, 0.653263 }, { 0.288657, 0.408284, 0.866014 }, { 0.270573, 0.923894, 0.270573 }, { -0.270573, 0.923894, 0.270573 }, { 0, 0.816538, 0.577292 }, { 0.923891, 0, 0.382655 }, { 0.866014, 0.408284, 0.288657 }, { 0.653263, 0.382747, 0.653263 }, { 0.923891, 0, -0.382655 }, { 0.653263, 0.382747, -0.653263 }, { 0.866014, 0.408284, -0.288657 }, { 0.270573, 0.923894, -0.270573 }, { 0.270573, 0.923894, 0.270573 }, { 0.577292, 0.816538, 0 }, { 0.382655, 0, -0.923891 }, { 0.288657, 0.408284, -0.866014 }, { 0.653263, 0.382747, -0.653263 }, { -0.382655, 0, -0.923891 }, { -0.653263, 0.382747, -0.653263 }, { -0.288657, 0.408284, -0.866014 }, { -0.270573, 0.923894, -0.270573 }, { 0.270573, 0.923894, -0.270573 }, { 0, 0.816538, -0.577292 }, { -0.923891, 0, -0.382655 }, { -0.866014, 0.408284, -0.288657 }, { -0.653263, 0.382747, -0.653263 }, { -0.923891, 0, 0.382655 }, { -0.653263, 0.382747, 0.653263 }, { -0.866014, 0.408284, 0.288657 }, { -0.270573, 0.923894, 0.270573 }, { -0.270573, 0.923894, -0.270573 }, { -0.577292, 0.816538, 0 }, { -0.653263, -0.382747, 0.653263 }, { -0.288657, -0.408284, 0.866014 }, { -0.382655, 0, 0.923891 }, { -0.270573, -0.923894, 0.270573 }, { 0.270573, -0.923894, 0.270573 }, { 0, -0.816538, 0.577292 }, { 0.653263, -0.382747, 0.653263 }, { 0.382655, 0, 0.923891 }, { 0.288657, -0.408284, 0.866014 }, { 0.653263, -0.382747, 0.653263 }, { 0.866014, -0.408284, 0.288657 }, { 0.923891, 0, 0.382655 }, { 0.270573, -0.923894, 0.270573 }, { 0.270573, -0.923894, -0.270573 }, { 0.577292, -0.816538, 0 }, { 0.653263, -0.382747, -0.653263 }, { 0.923891, 0, -0.382655 }, { 0.866014, -0.408284, -0.288657 }, { 0.653263, -0.382747, -0.653263 }, { 0.288657, -0.408284, -0.866014 }, { 0.382655, 0, -0.923891 }, { 0.270573, -0.923894, -0.270573 }, { -0.270573, -0.923894, -0.270573 }, { 0, -0.816538, -0.577292 }, { -0.653263, -0.382747, -0.653263 }, { -0.382655, 0, -0.923891 }, { -0.288657, -0.408284, -0.866014 }, { -0.653263, -0.382747, -0.653263 }, { -0.866014, -0.408284, -0.288657 }, { -0.923891, 0, -0.382655 }, { -0.270573, -0.923894, -0.270573 }, { -0.270573, -0.923894, 0.270573 }, { -0.577292, -0.816538, 0 }, { -0.653263, -0.382747, 0.653263 }, { -0.923891, 0, 0.382655 }, { -0.866014, -0.408284, 0.288657 } }, { 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 6, 30, 32, 30, 7, 31, 31, 8, 32, 9, 33, 35, 33, 10, 34, 34, 11, 35, 12, 36, 38, 36, 13, 37, 37, 14, 38, 15, 39, 41, 39, 16, 40, 40, 17, 41, 18, 42, 44, 42, 19, 43, 43, 20, 44, 21, 45, 47, 45, 22, 46, 46, 23, 47, 24, 48, 50, 48, 25, 49, 49, 26, 50, 27, 51, 53, 51, 28, 52, 52, 29, 53, 2, 54, 56, 54, 6, 55, 55, 8, 56, 6, 57, 59, 57, 3, 58, 58, 7, 59, 7, 60, 62, 60, 0, 61, 61, 8, 62, 3, 63, 65, 63, 9, 64, 64, 11, 65, 9, 66, 68, 66, 4, 67, 67, 10, 68, 10, 69, 71, 69, 0, 70, 70, 11, 71, 4, 72, 74, 72, 12, 73, 73, 14, 74, 12, 75, 77, 75, 5, 76, 76, 13, 77, 13, 78, 80, 78, 0, 79, 79, 14, 80, 5, 81, 83, 81, 15, 82, 82, 17, 83, 15, 84, 86, 84, 2, 85, 85, 16, 86, 16, 87, 89, 87, 0, 88, 88, 17, 89, 2, 90, 92, 90, 18, 91, 91, 20, 92, 18, 93, 95, 93, 1, 94, 94, 19, 95, 19, 96, 98, 96, 3, 97, 97, 20, 98, 3, 99, 101, 99, 21, 100, 100, 23, 101, 21, 102, 104, 102, 1, 103, 103, 22, 104, 22, 105, 107, 105, 4, 106, 106, 23, 107, 4, 108, 110, 108, 24, 109, 109, 26, 110, 24, 111, 113, 111, 1, 112, 112, 25, 113, 25, 114, 116, 114, 5, 115, 115, 26, 116, 5, 117, 119, 117, 27, 118, 118, 29, 119, 27, 120, 122, 120, 1, 121, 121, 28, 122, 28, 123, 125, 123, 2, 124, 124, 29, 125 });

  write_shader_asset(fout, CoreAsset::geometry_vert, "../../data/geometry.vert");
  write_shader_asset(fout, CoreAsset::geometry_frag, "../../data/geometry.frag");

  write_shader_asset(fout, CoreAsset::sprite_vert, "../../data/sprite.vert");
  write_shader_asset(fout, CoreAsset::sprite_frag, "../../data/sprite.frag");

  write_material_asset(fout, CoreAsset::default_material, Color3(0.64, 0.64, 0.64), "", Color3(0.0, 0.0, 0.0), 96.0, "", "");

  write_sprite_asset(fout, CoreAsset::loader_image, "../../data/loader.png", 8);

  write_sprite_asset(fout, CoreAsset::test_image, "../../data/testimage.png");

  write_font_asset(fout, CoreAsset::debug_font, "Comic Sans MS", 10, 50);

  write_chunk(fout, "HEND", 0, nullptr);

  fout.close();
}

int main(int argc, char **argv)
{
  QGuiApplication app(argc, argv);

  cout << "Asset Builder" << endl;

  try
  {
    write_core();

    write_mesh("plane.pack", "../../data/plane.obj");
    write_mesh("sphere.pack", "../../data/sphere.obj", 100);
  }
  catch(std::exception &e)
  {
    cerr << "Critical Error: " << e.what() << endl;
  }
}
