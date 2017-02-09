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
#include "hdr.h"
#include "ibl.h"
#include "glslang.h"
#include "assetpacker.h"
#include "atlaspacker.h"
#include <leap.h>
#include <fstream>
#include <unordered_map>
#include <cassert>

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
    vector<Vec3> tan1(vertices.size(), Vec3(0));
    vector<Vec3> tan2(vertices.size(), Vec3(0));

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
      auto t1 = v1.texcoord[1] - v2.texcoord[1];
      auto t2 = v1.texcoord[1] - v3.texcoord[1];

      auto r = s1 * t2 - s2 * t1;

      if (r != 0)
      {
        auto sdir = Vec3(t2 * x1 - t1 * x2, t2 * y1 - t1 * y2, t2 * z1 - t1 * z2) / r;
        auto tdir = Vec3(s1 * x2 - s2 * x1, s1 * y2 - s2 * y1, s1 * z2 - s2 * z1) / r;

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

      auto tangent = tan1[i];
      auto bitangent = tan2[i];

      orthonormalise(normal, tangent, bitangent);

      vertices[i].tangent[0] = tangent.x;
      vertices[i].tangent[1] = tangent.y;
      vertices[i].tangent[2] = tangent.z;
      vertices[i].tangent[3] = (dot(bitangent, tan2[i]) < 0.0f) ? -1.0f : 1.0f;
    }
  }
}


uint32_t write_catalog_asset(ostream &fout, uint32_t id, uint32_t magic, uint32_t version)
{
  write_catl_asset(fout, id, magic, version);

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

  cout << "  " << path << endl;

  auto spirv = compile_shader(text, stage);

  write_text_asset(fout, id, spirv.size(), spirv.data());

  return id + 1;
}


uint32_t write_image_asset(ostream &fout, uint32_t id, string const &path)
{
  QImage image(path.c_str());

  if (image.isNull())
    throw runtime_error("Failed to load image - " + path);

  cout << "  " << path << endl;

  image = image.convertToFormat(QImage::Format_ARGB32);

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = 1;

  vector<char> payload(image_datasize(width, height, layers, levels));

  memcpy(payload.data(), image.bits(), image.byteCount());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgba, payload.data());

  return id + 1;
}


uint32_t write_sprite_asset(ostream &fout, uint32_t id, vector<QImage> const &images)
{
  int width = images.front().width();
  int height = images.front().height();
  int layers = images.size();
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(image_datasize(width, height, layers, levels));

  char *dst = payload.data();

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

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgba, payload.data());

  return id + 1;
}


uint32_t write_sprite_asset(ostream &fout, uint32_t id, vector<string> const &paths)
{
  vector<QImage> layers;

  for(auto &path : paths)
  {
    QImage image(path.c_str());

    if (image.isNull())
      throw runtime_error("Failed to load image - " + path);

    cout << "  " << path << endl;

    layers.push_back(image);
  }

  write_sprite_asset(fout, id, layers);

  return id + 1;
}


uint32_t write_sprite_asset(ostream &fout, uint32_t id, string const &path)
{
  write_sprite_asset(fout, id, vector<string>{ path });

  return id + 1;
}


uint32_t write_sprite_asset(ostream &fout, uint32_t id, string const &path, int count)
{
  QImage sheet(path.c_str());

  if (sheet.isNull())
    throw runtime_error("Failed to load image - " + path);

  cout << "  " << path << endl;

  vector<QImage> layers;

  int width = sheet.width() / count;
  int height = sheet.height();

  for(int i = 0; i < sheet.width(); i += width)
  {
    layers.push_back(sheet.copy(i, 0, width, height));
  }

  write_sprite_asset(fout, id, layers);

  return id + 1;
}


uint32_t write_albedomap_asset(ostream &fout, uint32_t id, string const &path)
{
  QImage image(path.c_str());

  if (image.isNull())
    throw runtime_error("Failed to load image - " + path);

  cout << "  " << path << endl;

  image = image.convertToFormat(QImage::Format_ARGB32).mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(image_datasize(width, height, layers, levels));

  memcpy(payload.data(), image.bits(), image.byteCount());

  image_buildmips_srgb_a(0.5, width, height, layers, levels, payload.data());

  image_compress_bc3(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgba_bc3, payload.data());

  return id + 1;
}


uint32_t write_specularmap_asset(ostream &fout, uint32_t id, string const &path)
{
  QImage image(path.c_str());

  if (image.isNull())
    throw runtime_error("Failed to load image - " + path);

  cout << "  " << path << endl;

  image = image.convertToFormat(QImage::Format_ARGB32).mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(image_datasize(width, height, layers, levels));

  memcpy(payload.data(), image.bits(), image.byteCount());

  image_buildmips_rgb(width, height, layers, levels, payload.data());

  image_compress_bc3(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgba_bc3, payload.data());

  return id + 1;
}

uint32_t write_specularmap_asset(ostream &fout, uint32_t id, string const &metalpath, string const &roughpath)
{
  QImage roughmap(roughpath.c_str());

  if (roughmap.isNull())
    throw runtime_error("Failed to load image - " + roughpath);

  QImage metalmap(metalpath.c_str());

  if (metalmap.isNull())
    throw runtime_error("Failed to load image - " + metalpath);

  if (roughmap.size() != metalmap.size())
    throw runtime_error("Image size mismatch - " + roughpath + "," + metalpath);

  cout << "  " << metalpath << endl;
  cout << "  " << roughpath << endl;

  QImage image(roughmap.width(), roughmap.height(), QImage::Format_ARGB32);

  for(int y = 0; y < image.height(); ++y)
  {
    for(int x = 0; x < image.width(); ++x)
    {
      auto metalness = qRed(metalmap.pixel(x, y)) / 255.0f;
      auto reflectivity = 1.0f;
      auto roughness = qRed(roughmap.pixel(x, y)) / 255.0f;

      image.setPixel(x, y, qRgba(metalness * 255, reflectivity * 255, 0, roughness * 255));
    }
  }

  image = image.mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(image_datasize(width, height, layers, levels));

  memcpy(payload.data(), image.bits(), image.byteCount());

  image_buildmips_rgb(width, height, layers, levels, payload.data());

  image_compress_bc3(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgba_bc3, payload.data());

  return id + 1;
}


uint32_t write_normalmap_asset(ostream &fout, uint32_t id, string const &path)
{
  QImage image(path.c_str());

  if (image.isNull())
    throw runtime_error("Failed to load image - " + path);

  cout << "  " << path << endl;

  image = image.convertToFormat(QImage::Format_ARGB32).mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(image_datasize(width, height, layers, levels));

  memcpy(payload.data(), image.bits(), image.byteCount());

  image_buildmips_rgb(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgba, payload.data());

  return id + 1;
}


uint32_t write_skybox_asset(ostream &fout, uint32_t id, vector<string> const &paths)
{
  vector<QImage> images;

  for(auto &path : paths)
  {
    QImage image(path.c_str());

    if (image.isNull())
      throw runtime_error("Failed to load image - " + path);

    cout << "  " << path << endl;

    images.push_back(image);
  }

  assert(images.size() == 6);

  int width = images.front().width();
  int height = images.front().height();
  int layers = 6;
  int levels = 8;

  vector<char> payload(image_datasize(width, height, layers, levels));

  char *dst = payload.data();

  for(size_t i = 0; i < images.size(); i++)
  {
    QImage image = images[i].convertToFormat(QImage::Format_ARGB32);

    if (image.width() != width || image.height() != height)
      throw runtime_error("Layers with differing dimensions");

    for(int y = 0; y < image.height(); ++y)
    {
      for(int x = 0; x < image.width(); ++x)
      {
        image.setPixel(x, y, rgbe(srgba(image.pixel(x, y))));
      }
    }

    image = image.mirrored();

    memcpy(dst, image.bits(), image.byteCount());

    dst += image.byteCount();
  }

  image_buildmips_cube_ibl(width, height, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgbe, payload.data());

  return id + 1;
}


uint32_t write_skybox_asset(ostream &fout, uint32_t id, string const &path)
{
  auto image = load_hdr(path);

  cout << "  " << path << endl;

  int width = 512;
  int height = 512;
  int layers = 6;
  int levels = 8;

  vector<char> payload(image_datasize(width, height, layers, levels));

  image_pack_cube_ibl(image, width, height, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgbe, payload.data());

  return id + 1;
}


uint32_t write_envbrdf_asset(ostream &fout, uint32_t id)
{
  int width = 256;
  int height = 256;
  int layers = 1;
  int levels = 1;

  vector<char> payload(image_datasize(width, height, layers, levels));

  image_pack_envbrdf(width, height, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgbe, payload.data());

  return id + 1;
}


uint32_t write_watermap_asset(ostream &fout, uint32_t id, Color3 const &deepcolor, Color3 const &shallowcolor, float scalepower = 1.0f, Color3 const &fresnelcolor = { 0.0f, 0.0f, 0.0f }, float fresnelbias = 0.328f, float fresnelpower = 5.0f)
{
  int width = 256;
  int height = 256;
  int layers = 1;
  int levels = 1;

  vector<char> payload(image_datasize(width, height, layers, levels));

  image_pack_watercolor(deepcolor, shallowcolor, scalepower, fresnelcolor, fresnelbias, fresnelpower, width, height, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgbe, payload.data());

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

  cout << "  " << path << endl;

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

  return id + 1;
}


uint32_t write_material_asset(ostream &fout, uint32_t id, Color3 color, float metalness, float roughness, float reflectivity, float emissive, string albedomap, string specularmap, string normalmap)
{
  int mapid = 0;
  uint32_t albedomapid = (albedomap != "") ? ++mapid : 0;
  uint32_t specularmapid = (specularmap != "") ? ++mapid : 0;
  uint32_t normalmapid = (normalmap != "") ? ++mapid : 0;

  write_matl_asset(fout, id, color, metalness, roughness, reflectivity, emissive, albedomapid, specularmapid, normalmapid);

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

  cout << "  " << fontname << endl;

  int count = 127;

  vector<uint16_t> x(count);
  vector<uint16_t> y(count);
  vector<uint16_t> width(count);
  vector<uint16_t> height(count);
  vector<int16_t> offsetx(count);
  vector<int16_t> offsety(count);
  vector<uint8_t> advance(count*count);

  AtlasPacker packer(512, 256);

  for(int codepoint = 33; codepoint < count; ++codepoint)
  {
    auto position = packer.insert(codepoint, tm.width(QString(codepoint)) - tm.leftBearing(codepoint) - tm.rightBearing(codepoint) + 4, tm.height() + 4);

    assert(position);

    x[codepoint] = position->x;
    y[codepoint] = position->y;
    width[codepoint] = position->width - 1;
    height[codepoint] = position->height - 1;
    offsetx[codepoint] = 1 - tm.leftBearing(codepoint);
    offsety[codepoint] = 1 + tm.ascent();
  }

  for(int codepoint = 0; codepoint < count; ++codepoint)
  {
    advance[codepoint] = 0;

    for(int othercodepoint = 1; othercodepoint < count; ++othercodepoint)
    {
      advance[othercodepoint * count + codepoint] = tm.width(QString(othercodepoint) + QString(codepoint)) - tm.width(QString(codepoint));
    }
  }

  write_font_asset(fout, id, tm.ascent(), tm.descent(), tm.leading(), count, 1, x, y, width, height, offsetx, offsety, advance);

  QImage atlas(packer.width, packer.height, QImage::Format_ARGB32);

  atlas.fill(0x00000000);

  for(int codepoint = 33; codepoint < count; ++codepoint)
  {
    auto position = packer.find(codepoint);

    QPainter painter(&atlas);

    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(position->x + offsetx[codepoint] + 1, position->y + offsety[codepoint] + 1, QString(codepoint));
  }

  int levels = min(4, image_maxlevels(atlas.width(), atlas.height()));

  vector<char> payload(image_datasize(atlas.width(), atlas.height(), 1, levels));

  memcpy(payload.data(), atlas.bits(), atlas.byteCount());

  image_buildmips_srgb(atlas.width(), atlas.height(), 1, levels, payload.data());

  write_imag_asset(fout, id+1, atlas.width(), atlas.height(), 1, levels, PackImageHeader::rgba, payload.data());

  return id + 2;
}


void write_material(string const &output, Color3 color, float metalness, float roughness, float reflectivity, float emissive, string albedomap, string specularmap, string normalmap)
{
  ofstream fout(output, ios::binary | ios::trunc);

  write_header(fout);

  write_material_asset(fout, 0, color, metalness, roughness, reflectivity, emissive, albedomap, specularmap, normalmap);

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

  write_catalog_asset(fout, CoreAsset::catalog, CoreAsset::magic, CoreAsset::version);

  write_image_asset(fout, CoreAsset::white_diffuse, "../../data/white.png");
  write_image_asset(fout, CoreAsset::nominal_normal, "../../data/normal.png");

  write_mesh_asset(fout, CoreAsset::unit_quad, { { -1.0, 1.0, 0.0, 0.0, 1.0 }, { -1.0, -1.0, 0.0, 0.0, 0.0 }, { 1.0, 1.0, 0.0, 1.0, 1.0 }, { 1.0, -1.0, 0.0, 1.0, 0.0 } }, { 0, 1, 2, 2, 1, 3 });
  write_mesh_asset(fout, CoreAsset::unit_cube, { { -1.0, -1.0, 1.0 }, { 1.0, -1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { -1.0, 1.0, 1.0 }, { -1.0, -1.0, -1.0 }, { 1.0, -1.0, -1.0 }, { 1.0, 1.0, -1.0 }, { -1.0, 1.0, -1.0 } }, { 0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4, 3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4 });
  write_mesh_asset(fout, CoreAsset::unit_cone, { { 0.0, 0.0, 0.0 }, { 0.0, 1.0, -1.0 }, { -0.5f, 0.866, -1.0 }, { -0.866, 0.5, -1.0 }, { -1.0, 0.0, -1.0 }, { -0.866, -0.5, -1.0 }, { -0.5, -0.866, -1.0 }, { 0.0, -1.0, -1.0 }, { 0.5, -0.866, -1.0 }, { 0.866, -0.5, -1.0 }, { 1.0, 0.0, -1.0 }, { 0.866, 0.5, -1.0 }, { 0.5f, 0.866, -1.0 } }, { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 6, 0, 6, 7, 0, 7, 8, 0, 8, 9, 0, 9, 10, 0, 10, 11, 0, 11, 12, 0, 12, 1, 10, 6, 2, 10, 8, 6, 10, 9, 8, 8, 7, 6, 6, 4, 2, 6, 5, 4, 4, 3, 2, 2, 12, 10, 2, 1, 12, 12, 11, 10 });
  write_mesh_asset(fout, CoreAsset::unit_sphere, { { 0, 1, 0 }, { 0, -1, 0 }, { -0.707, 0, 0.707 }, { 0.707, 0, 0.707 }, { 0.707, 0, -0.707 }, { -0.707, 0, -0.707 }, { 0, 0, 1 }, { 0.499962, 0.70716, 0.499962 }, { -0.499962, 0.70716, 0.499962 }, { 1, 0, 0 }, { 0.499962, 0.70716, -0.499962 }, { 0.499962, 0.70716, 0.499962 }, { 0, 0, -1 }, { -0.499962, 0.70716, -0.499962 }, { 0.499962, 0.70716, -0.499962 }, { -1, 0, 0 }, { -0.499962, 0.70716, 0.499962 }, { -0.499962, 0.70716, -0.499962 }, { -0.499962, -0.70716, 0.499962 }, { 0.499962, -0.70716, 0.499962 }, { 0, 0, 1 }, { 0.499962, -0.70716, 0.499962 }, { 0.499962, -0.70716, -0.499962 }, { 1, 0, 0 }, { 0.499962, -0.70716, -0.499962 }, { -0.499962, -0.70716, -0.499962 }, { 0, 0, -1 }, { -0.499962, -0.70716, -0.499962 }, { -0.499962, -0.70716, 0.499962 }, { -1, 0, 0 }, { 0.288657, 0.408284, 0.866014 }, { 0, 0.816538, 0.577292 }, { -0.288657, 0.408284, 0.866014 }, { 0.866014, 0.408284, -0.288657 }, { 0.577292, 0.816538, 0 }, { 0.866014, 0.408284, 0.288657 }, { -0.288657, 0.408284, -0.866014 }, { 0, 0.816538, -0.577292 }, { 0.288657, 0.408284, -0.866014 }, { -0.866014, 0.408284, 0.288657 }, { -0.577292, 0.816538, 0 }, { -0.866014, 0.408284, -0.288657 }, { 0, -0.816538, 0.577292 }, { 0.288657, -0.408284, 0.866014 }, { -0.288657, -0.408284, 0.866014 }, { 0.577292, -0.816538, 0 }, { 0.866014, -0.408284, -0.288657 }, { 0.866014, -0.408284, 0.288657 }, { 0, -0.816538, -0.577292 }, { -0.288657, -0.408284, -0.866014 }, { 0.288657, -0.408284, -0.866014 }, { -0.577292, -0.816538, 0 }, { -0.866014, -0.408284, 0.288657 }, { -0.866014, -0.408284, -0.288657 }, { -0.382655, 0, 0.923891 }, { -0.288657, 0.408284, 0.866014 }, { -0.653263, 0.382747, 0.653263 }, { 0.382655, 0, 0.923891 }, { 0.653263, 0.382747, 0.653263 }, { 0.288657, 0.408284, 0.866014 }, { 0.270573, 0.923894, 0.270573 }, { -0.270573, 0.923894, 0.270573 }, { 0, 0.816538, 0.577292 }, { 0.923891, 0, 0.382655 }, { 0.866014, 0.408284, 0.288657 }, { 0.653263, 0.382747, 0.653263 }, { 0.923891, 0, -0.382655 }, { 0.653263, 0.382747, -0.653263 }, { 0.866014, 0.408284, -0.288657 }, { 0.270573, 0.923894, -0.270573 }, { 0.270573, 0.923894, 0.270573 }, { 0.577292, 0.816538, 0 }, { 0.382655, 0, -0.923891 }, { 0.288657, 0.408284, -0.866014 }, { 0.653263, 0.382747, -0.653263 }, { -0.382655, 0, -0.923891 }, { -0.653263, 0.382747, -0.653263 }, { -0.288657, 0.408284, -0.866014 }, { -0.270573, 0.923894, -0.270573 }, { 0.270573, 0.923894, -0.270573 }, { 0, 0.816538, -0.577292 }, { -0.923891, 0, -0.382655 }, { -0.866014, 0.408284, -0.288657 }, { -0.653263, 0.382747, -0.653263 }, { -0.923891, 0, 0.382655 }, { -0.653263, 0.382747, 0.653263 }, { -0.866014, 0.408284, 0.288657 }, { -0.270573, 0.923894, 0.270573 }, { -0.270573, 0.923894, -0.270573 }, { -0.577292, 0.816538, 0 }, { -0.653263, -0.382747, 0.653263 }, { -0.288657, -0.408284, 0.866014 }, { -0.382655, 0, 0.923891 }, { -0.270573, -0.923894, 0.270573 }, { 0.270573, -0.923894, 0.270573 }, { 0, -0.816538, 0.577292 }, { 0.653263, -0.382747, 0.653263 }, { 0.382655, 0, 0.923891 }, { 0.288657, -0.408284, 0.866014 }, { 0.653263, -0.382747, 0.653263 }, { 0.866014, -0.408284, 0.288657 }, { 0.923891, 0, 0.382655 }, { 0.270573, -0.923894, 0.270573 }, { 0.270573, -0.923894, -0.270573 }, { 0.577292, -0.816538, 0 }, { 0.653263, -0.382747, -0.653263 }, { 0.923891, 0, -0.382655 }, { 0.866014, -0.408284, -0.288657 }, { 0.653263, -0.382747, -0.653263 }, { 0.288657, -0.408284, -0.866014 }, { 0.382655, 0, -0.923891 }, { 0.270573, -0.923894, -0.270573 }, { -0.270573, -0.923894, -0.270573 }, { 0, -0.816538, -0.577292 }, { -0.653263, -0.382747, -0.653263 }, { -0.382655, 0, -0.923891 }, { -0.288657, -0.408284, -0.866014 }, { -0.653263, -0.382747, -0.653263 }, { -0.866014, -0.408284, -0.288657 }, { -0.923891, 0, -0.382655 }, { -0.270573, -0.923894, -0.270573 }, { -0.270573, -0.923894, 0.270573 }, { -0.577292, -0.816538, 0 }, { -0.653263, -0.382747, 0.653263 }, { -0.923891, 0, 0.382655 }, { -0.866014, -0.408284, 0.288657 } }, { 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 6, 30, 32, 30, 7, 31, 31, 8, 32, 9, 33, 35, 33, 10, 34, 34, 11, 35, 12, 36, 38, 36, 13, 37, 37, 14, 38, 15, 39, 41, 39, 16, 40, 40, 17, 41, 18, 42, 44, 42, 19, 43, 43, 20, 44, 21, 45, 47, 45, 22, 46, 46, 23, 47, 24, 48, 50, 48, 25, 49, 49, 26, 50, 27, 51, 53, 51, 28, 52, 52, 29, 53, 2, 54, 56, 54, 6, 55, 55, 8, 56, 6, 57, 59, 57, 3, 58, 58, 7, 59, 7, 60, 62, 60, 0, 61, 61, 8, 62, 3, 63, 65, 63, 9, 64, 64, 11, 65, 9, 66, 68, 66, 4, 67, 67, 10, 68, 10, 69, 71, 69, 0, 70, 70, 11, 71, 4, 72, 74, 72, 12, 73, 73, 14, 74, 12, 75, 77, 75, 5, 76, 76, 13, 77, 13, 78, 80, 78, 0, 79, 79, 14, 80, 5, 81, 83, 81, 15, 82, 82, 17, 83, 15, 84, 86, 84, 2, 85, 85, 16, 86, 16, 87, 89, 87, 0, 88, 88, 17, 89, 2, 90, 92, 90, 18, 91, 91, 20, 92, 18, 93, 95, 93, 1, 94, 94, 19, 95, 19, 96, 98, 96, 3, 97, 97, 20, 98, 3, 99, 101, 99, 21, 100, 100, 23, 101, 21, 102, 104, 102, 1, 103, 103, 22, 104, 22, 105, 107, 105, 4, 106, 106, 23, 107, 4, 108, 110, 108, 24, 109, 109, 26, 110, 24, 111, 113, 111, 1, 112, 112, 25, 113, 25, 114, 116, 114, 5, 115, 115, 26, 116, 5, 117, 119, 117, 27, 118, 118, 29, 119, 27, 120, 122, 120, 1, 121, 121, 28, 122, 28, 123, 125, 123, 2, 124, 124, 29, 125 });

  write_mesh_asset(fout, CoreAsset::line_quad, { { -1.0, 1.0, 0.0 }, { -1.0, -1.0, 0.0 }, { 1.0, -1.0, 0.0 }, { 1.0, 1.0, 0.0 } }, { 0, 1, 1, 2, 2, 3, 3, 0 });
  write_mesh_asset(fout, CoreAsset::line_cube, { { -1.0, -1.0, 1.0 }, { 1.0, -1.0, 1.0 }, { 1.0, 1.0, 1.0 }, { -1.0, 1.0, 1.0 }, { -1.0, -1.0, -1.0 }, { 1.0, -1.0, -1.0 }, { 1.0, 1.0, -1.0 }, { -1.0, 1.0, -1.0 } }, { 0, 1, 1, 2, 2, 3, 3, 0, 0, 4, 1, 5, 2, 6, 3, 7, 4, 5, 5, 6, 6, 7, 7, 4 });

  write_shader_asset(fout, CoreAsset::shadow_vert, "../../data/shadow.vert");
  write_shader_asset(fout, CoreAsset::shadow_geom, "../../data/shadow.geom");
  write_shader_asset(fout, CoreAsset::shadow_frag, "../../data/shadow.frag");

  write_shader_asset(fout, CoreAsset::depth_vert, "../../data/depth.vert");
  write_shader_asset(fout, CoreAsset::depth_frag, "../../data/depth.frag");

  write_shader_asset(fout, CoreAsset::geometry_vert, "../../data/geometry.vert");
  write_shader_asset(fout, CoreAsset::geometry_frag, "../../data/geometry.frag");

  write_shader_asset(fout, CoreAsset::ocean_vert, "../../data/ocean.vert");
  write_shader_asset(fout, CoreAsset::ocean_frag, "../../data/ocean.frag");

  write_shader_asset(fout, CoreAsset::fogplane_vert, "../../data/fogplane.vert");
  write_shader_asset(fout, CoreAsset::fogplane_frag, "../../data/fogplane.frag");

  write_shader_asset(fout, CoreAsset::translucent_vert, "../../data/translucent.vert");
  write_shader_asset(fout, CoreAsset::translucent_frag, "../../data/translucent.frag");

  write_shader_asset(fout, CoreAsset::particle_vert, "../../data/particle.vert");
  write_shader_asset(fout, CoreAsset::particle_frag, "../../data/particle.frag");

  write_shader_asset(fout, CoreAsset::ssao_comp, "../../data/ssao.comp");

  write_envbrdf_asset(fout, CoreAsset::envbrdf_lut);

  write_shader_asset(fout, CoreAsset::lighting_comp, "../../data/lighting.comp");

  write_shader_asset(fout, CoreAsset::ssr_comp, "../../data/ssr.comp");

  write_shader_asset(fout, CoreAsset::skybox_vert, "../../data/skybox.vert");
  write_shader_asset(fout, CoreAsset::skybox_frag, "../../data/skybox.frag");
  write_shader_asset(fout, CoreAsset::skybox_comp, "../../data/skybox.comp");
//  write_skybox_asset(fout, CoreAsset::default_skybox, { "../../data/skybox_rt.jpg", "../../data/skybox_lf.jpg", "../../data/skybox_dn.jpg", "../../data/skybox_up.jpg", "../../data/skybox_fr.jpg", "../../data/skybox_bk.jpg" });
//  write_skybox_asset(fout, CoreAsset::default_skybox, "../../data/pisa.hdr");
  write_skybox_asset(fout, CoreAsset::default_skybox, "../../data/Serpentine_Valley_3k.hdr");

  write_shader_asset(fout, CoreAsset::bloom_luma_comp, "../../data/bloom.luma.comp");
  write_shader_asset(fout, CoreAsset::bloom_hblur_comp, "../../data/bloom.hblur.comp");
  write_shader_asset(fout, CoreAsset::bloom_vblur_comp, "../../data/bloom.vblur.comp");

  write_shader_asset(fout, CoreAsset::luminance_comp, "../../data/luminance.comp");

  write_shader_asset(fout, CoreAsset::composite_vert, "../../data/composite.vert");
  write_shader_asset(fout, CoreAsset::composite_frag, "../../data/composite.frag");

  write_shader_asset(fout, CoreAsset::sprite_vert, "../../data/sprite.vert");
  write_shader_asset(fout, CoreAsset::sprite_frag, "../../data/sprite.frag");

  write_shader_asset(fout, CoreAsset::gizmo_vert, "../../data/gizmo.vert");
  write_shader_asset(fout, CoreAsset::gizmo_frag, "../../data/gizmo.frag");

  write_shader_asset(fout, CoreAsset::wireframe_vert, "../../data/wireframe.vert");
  write_shader_asset(fout, CoreAsset::wireframe_geom, "../../data/wireframe.geom");
  write_shader_asset(fout, CoreAsset::wireframe_frag, "../../data/wireframe.frag");

  write_shader_asset(fout, CoreAsset::stencilmask_vert, "../../data/stencilmask.vert");
  write_shader_asset(fout, CoreAsset::stencilmask_frag, "../../data/stencilmask.frag");

  write_shader_asset(fout, CoreAsset::stencilfill_vert, "../../data/stencilfill.vert");
  write_shader_asset(fout, CoreAsset::stencilfill_frag, "../../data/stencilfill.frag");

  write_shader_asset(fout, CoreAsset::stencilpath_vert, "../../data/stencilpath.vert");
  write_shader_asset(fout, CoreAsset::stencilpath_geom, "../../data/stencilpath.geom");
  write_shader_asset(fout, CoreAsset::stencilpath_frag, "../../data/stencilpath.frag");

  write_shader_asset(fout, CoreAsset::line_vert, "../../data/line.vert");
  write_shader_asset(fout, CoreAsset::line_geom, "../../data/line.geom");
  write_shader_asset(fout, CoreAsset::line_frag, "../../data/line.frag");

  write_shader_asset(fout, CoreAsset::outline_vert, "../../data/outline.vert");
  write_shader_asset(fout, CoreAsset::outline_geom, "../../data/outline.geom");
  write_shader_asset(fout, CoreAsset::outline_frag, "../../data/outline.frag");

  write_watermap_asset(fout, CoreAsset::wave_color, Color3(0.0, 0.025, 0.046), Color3(0.1, 0.1, 0.1), 1.0);
  write_normalmap_asset(fout, CoreAsset::wave_normal, "../../data/wavenormal.png");

  write_material_asset(fout, CoreAsset::default_material, Color3(0.64, 0.64, 0.64), 0, 1, 0.5, 0.0, "", "", "");

  write_sprite_asset(fout, CoreAsset::default_particle, "../../data/particle.png");

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
    write_mesh("sphere.pack", "../../data/sphere.obj");

    write_mesh("cube.pack", "../../data/cube.obj");
    write_mesh("teapot.pack", "../../data/teapot.obj");
    write_mesh("suzanne.pack", "../../data/suzanne.obj");
  }
  catch(exception &e)
  {
    cerr << "Critical Error: " << e.what() << endl;
  }
}
