//
// Datum - obj asset builder
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include <QGuiApplication>
#include <QImage>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include "assetpacker.h"
#include <leap.h>
#include <leap/pathstring.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>

using namespace std;
using namespace lml;
using namespace leap;

float gScale = 1.0f;

namespace
{
  vector<string_view> seperate(string_view str, const char *delimiters = " \t\r\n")
  {
    vector<string_view> result;

    size_t i = 0;
    size_t j = 0;

    while (j != string_view::npos)
    {
      j = str.find_first_of(delimiters, i);

      result.push_back(str.substr(i, j-i));

      i = j + 1;
    }

    return result;
  }


  void calculate_tangents(vector<PackVertex> &vertices, vector<uint32_t> &indices)
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


uint32_t write_diffmap_asset(ostream &fout, uint32_t id, string const &path, string const &mask)
{
  QImage image(path.c_str());

  if (image.isNull())
    throw runtime_error("Failed to load image - " + path);

  image = image.convertToFormat(QImage::Format_ARGB32);

  if (mask != "")
  {
    QImage alpha(mask.c_str());

    if (alpha.isNull())
      throw runtime_error("Failed to load image - " + mask);

    if (alpha.size() != image.size())
      throw runtime_error("Image size mismatch - " + mask);

    for(int y = 0; y < image.height(); ++y)
    {
      for(int x = 0; x < image.width(); ++x)
      {
        image.setPixel(x, y, (image.pixel(x, y) & 0x00FFFFFF) | ((alpha.pixel(x, y) & 0xFF) < 16 ? 0 : 0xFF) << 24);
      }
    }
  }

  image = image.mirrored();

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


uint32_t write_specmap_asset(ostream &fout, uint32_t id, string const &metalpath, string const &roughpath)
{
  QImage roughmap(roughpath.c_str());

  if (roughmap.isNull())
    throw runtime_error("Failed to load image - " + roughpath);

  QImage metalmap(metalpath.c_str());

  if (metalmap.isNull())
    throw runtime_error("Failed to load image - " + metalpath);

  if (roughmap.size() != metalmap.size())
    throw runtime_error("Image size mismatch - " + roughpath + "," + metalpath);

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
  int levels = image_maxlevels(width, height);

  vector<char> payload(image_datasize(width, height, layers, levels));

  memcpy(payload.data(), image.bits(), image.byteCount());

  image_buildmips_rgb(width, height, layers, levels, payload.data());

  image_compress_bc3(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgba_bc3, payload.data());

  return id + 1;
}


uint32_t write_bumpmap_asset(ostream &fout, uint32_t id, string const &path, float strength = 1.0f)
{
  QImage src(path.c_str());

  if (src.isNull())
    throw runtime_error("Failed to load image - " + path);

  QImage image = src.convertToFormat(QImage::Format_ARGB32);

  for(int y = 0; y < src.height(); ++y)
  {
    for(int x = 0; x < src.width(); ++x)
    {
      QColor grid[9] = {
        src.pixel((x+src.width()-1)%src.width(), (y+src.height()-1)%src.height()),
        src.pixel((x+src.width()  )%src.width(), (y+src.height()-1)%src.height()),
        src.pixel((x+src.width()+1)%src.width(), (y+src.height()-1)%src.height()),
        src.pixel((x+src.width()-1)%src.width(), (y+src.height()  )%src.height()),
        src.pixel((x+src.width()  )%src.width(), (y+src.height()  )%src.height()),
        src.pixel((x+src.width()+1)%src.width(), (y+src.height()  )%src.height()),
        src.pixel((x+src.width()-1)%src.width(), (y+src.height()+1)%src.height()),
        src.pixel((x+src.width()  )%src.width(), (y+src.height()+1)%src.height()),
        src.pixel((x+src.width()+1)%src.width(), (y+src.height()+1)%src.height())
      };

      float s[9];
      for(int i = 0; i < 9; ++i)
        s[i] = (grid[i].redF() + grid[i].greenF() + grid[i].blueF()) / 3.0;

      Vec3 normal = normalise(Vec3((s[2] + 2*s[5] + s[8]) - (s[0] + 2*s[3] + s[6]), (s[0] + 2*s[1] + s[2]) - (s[6] + 2*s[7] + s[8]), 1/strength));

      image.setPixel(x, y, qRgba((0.5f*normal.x+0.5f)*255, (0.5f*normal.y+0.5f)*255, (0.5f*normal.z+0.5f)*255, 255));
    }
  }

  image = image.mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = image_maxlevels(width, height);

  vector<char> payload(image_datasize(width, height, layers, levels));

  memcpy(payload.data(), image.bits(), image.byteCount());

  image_buildmips_rgb(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgba, payload.data());

  return id + 1;
}


uint32_t write_normalmap_asset(ostream &fout, uint32_t id, string const &path)
{
  QImage src(path.c_str());

  if (src.isNull())
    throw runtime_error("Failed to load image - " + path);

  QImage image = src.convertToFormat(QImage::Format_ARGB32).mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = image_maxlevels(width, height);

  vector<char> payload(image_datasize(width, height, layers, levels));

  memcpy(payload.data(), image.bits(), image.byteCount());

  image_buildmips_rgb(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, PackImageHeader::rgba, payload.data());

  return id + 1;
}


void write_model(pathstring const &src, pathstring const &dst)
{
  vector<Vec3> points;
  vector<Vec3> normals;
  vector<Vec2> texcoords;

  struct Texture
  {
    int type;
    string paths[2];
  };

  vector<Texture> textures;

  textures.push_back({ PackModelPayload::Texture::nullmap, "default" });

  struct Material
  {
    string name;

    Color3 color = Color3(1.0f, 1.0f, 1.0f);

    Color3 specularintensity = Color3(0.5f, 0.5f, 0.5f);
    float specularexponent = 96.0f;

    uint32_t albedomap = 0;
    uint32_t surfacemap = 0;
    uint32_t normalmap = 0;

    float disolve = 1.0f;

    string albedobase;
    string albedomask;
    string metalmap;
    string roughmap;
    string bumpmap;
  };

  vector<Material> materials;

  Material *material = nullptr;

  struct Mesh
  {
    string name;
    string usemtl;

    vector<PackVertex> vertices;
    vector<uint32_t> indices;
    unordered_map<PackVertex, uint32_t> vertexmap;

    uint32_t material;
    Transform transform;
  };

  string name;
  Transform transform;

  vector<Mesh> meshes;

  Mesh *mesh = nullptr;

  pathstring mtllib;

  string basepath = src.base();

  ifstream fin(src);

  if (!fin)
    throw runtime_error("Unable to read obj file - " + src.name());

  cout << "  Reading: " << src.name() << endl;

  string buffer;

  while (getline(fin, buffer))
  {
    auto line = trim(buffer);

    // skip comments
    if (line.empty() || line[0] == '#' || line[0] == '/')
      continue;

    auto fields = split(line);

    if (fields[0] == "mtllib")
    {
      mtllib = pathstring(basepath, fields[1]);
    }

    if (fields[0] == "usemtl")
    {
      meshes.push_back({});

      mesh = &meshes.back();

      mesh->name = name;
      mesh->usemtl = fields[1].to_string();
      mesh->transform = transform;
    }

    if (fields[0] == "o" || fields[0] == "g")
    {
      name = fields[1].to_string();
      transform = Transform::identity();
    }

    if (fields[0] == "v")
    {
      points.emplace_back(ato<float>(fields[1]), ato<float>(fields[2]), ato<float>(fields[3]));
    }

    if (fields[0] == "vn")
    {
      normals.emplace_back(ato<float>(fields[1]), ato<float>(fields[2]), ato<float>(fields[3]));
    }

    if (fields[0] == "vt")
    {
      texcoords.emplace_back(ato<float>(fields[1]), ato<float>(fields[2]));
    }

    if (fields[0] == "f")
    {
      if (!mesh)
        throw runtime_error("Object not specified");

      vector<string_view> face[] = { seperate(fields[1], "/"), seperate(fields[2], "/"), seperate(fields[3], "/") };

      for(auto &v : face)
      {
        PackVertex vertex = {};

        if (v.size() > 0 && ato<int>(v[0]) != 0)
          memcpy(vertex.position, &points[ato<int>(v[0]) - 1], sizeof(vertex.position));

        if (v.size() > 1 && ato<int>(v[1]) != 0)
          memcpy(vertex.texcoord, &texcoords[ato<int>(v[1]) - 1], sizeof(vertex.texcoord));

        if (v.size() > 2 && ato<int>(v[2]) != 0)
          memcpy(vertex.normal, &normals[ato<int>(v[2]) - 1], sizeof(vertex.normal));

        if (mesh->vertexmap.find(vertex) == mesh->vertexmap.end())
        {
          mesh->vertices.push_back(vertex);

          mesh->vertexmap[vertex] = mesh->vertices.size() - 1;
        }

        mesh->indices.push_back(mesh->vertexmap[vertex]);
      }
    }
  }

  cout << "  Parsing: " << src.name() << " (" << points.size() << " points, " << texcoords.size() << " texcoords, " << points.size() << " normals)" << endl;

  if (mtllib != "")
  {
    ifstream fin(mtllib);

    if (!fin)
      throw runtime_error("Unable to read mtl file - " + mtllib.name());

    cout << "  Reading: " << mtllib.name() << endl;

    string buffer;

    while (getline(fin, buffer))
    {
      auto line = trim(buffer);

      // skip comments
      if (line.empty() || line[0] == '#' || line[0] == '/')
        continue;

      auto fields = split(line);

      if (fields[0] == "newmtl")
      {
        materials.push_back({});

        material = &materials.back();

        material->name = fields[1].to_string();

        for(auto &mesh : meshes)
        {
          if (mesh.usemtl == material->name)
            mesh.material = indexof(materials, material);
        }
      }

      if (fields[0] == "Kd")
      {
        material->color.r = ato<float>(fields[1]);
        material->color.g = ato<float>(fields[2]);
        material->color.b = ato<float>(fields[3]);
      }

      if (fields[0] == "Ks")
      {
        material->specularintensity.r = ato<float>(fields[1]);
        material->specularintensity.g = ato<float>(fields[2]);
        material->specularintensity.b = ato<float>(fields[3]);
      }

      if (fields[0] == "Ns")
      {
        material->specularexponent = ato<float>(fields[1]);
      }

      if (fields[0] == "d")
      {
        material->disolve = ato<float>(fields[1]);
      }

      if (fields[0] == "map_d")
      {
        material->albedomask = ato<string>(fields[1]);
      }

      if (fields[0] == "map_Ka")
      {
        material->metalmap = ato<string>(fields[1]);
      }

      if (fields[0] == "map_Kd")
      {
        material->albedobase = ato<string>(fields[1]);
      }

      if (fields[0] == "map_Ns")
      {
        material->roughmap = ato<string>(fields[1]);
      }

      if (fields[0] == "map_bump" || fields[0] == "bump")
      {
        material->bumpmap = ato<string>(fields[1]);
      }
    }

    cout << "  Parsing: " << mtllib.name() << " (" << materials.size()-1 << " materials, " << textures.size()-1 << " textures)" << endl;
  }

  // Post Process

  cout << "  Procesing: " << "Scale " << gScale << ", Tangents, Draw Order" << endl;

  meshes.erase(remove_if(meshes.begin(), meshes.end(), [&](auto &mesh) { return materials[mesh.material].disolve < 0.5; }), meshes.end());

  for(auto &mesh : meshes)
  {
    for(auto &vertex : mesh.vertices)
    {
      vertex.position[0] *= gScale;
      vertex.position[1] *= gScale;
      vertex.position[2] *= gScale;
    }

    calculate_tangents(mesh.vertices, mesh.indices);
  }

  for(auto &material : materials)
  {
    if (material.albedobase != "")
    {
      auto j = find_if(textures.begin(), textures.end(), [&](auto &texture) { return (texture.type == PackModelPayload::Texture::albedomap && texture.paths[0] == material.albedobase && texture.paths[1] == material.albedomask); });

      if (j == textures.end())
      {
        textures.push_back({ PackModelPayload::Texture::albedomap, material.albedobase, material.albedomask });

        j = textures.end() - 1;
      }

      material.albedomap = j - textures.begin();
    }

    if (material.roughmap != "")
    {
      auto j = find_if(textures.begin(), textures.end(), [&](auto &texture) { return (texture.type == PackModelPayload::Texture::surfacemap && texture.paths[0] == material.roughmap && texture.paths[1] == material.metalmap); });

      if (j == textures.end())
      {
        textures.push_back({ PackModelPayload::Texture::surfacemap, material.roughmap, material.metalmap });

        j = textures.end() - 1;
      }

      material.surfacemap = j - textures.begin();
    }

    if (material.bumpmap != "")
    {
      auto j = find_if(textures.begin(), textures.end(), [&](auto &texture) { return (texture.type == PackModelPayload::Texture::normalmap && texture.paths[0] == material.bumpmap); });

      if (j == textures.end())
      {
        textures.push_back({ PackModelPayload::Texture::normalmap, material.bumpmap });

        j = textures.end() - 1;
      }

      material.normalmap = j - textures.begin();
    }
  }

  auto batchorder = [&](Mesh const &lhs) { return make_tuple(materials[lhs.material].albedomap, materials[lhs.material].normalmap); };

  sort(meshes.begin(), meshes.end(), [&](auto &lhs, auto &rhs) { return batchorder(lhs) < batchorder(rhs); });


  //
  // Output
  //

  uint32_t id = 0;

  ofstream fout(dst, ios::binary | ios::trunc);

  if (!fout)
    throw runtime_error("Error creating output file - " + src.name());

  write_header(fout);

  cout << "  Writing: (" << id << ") model " << dst.name() << endl;

  vector<PackModelPayload::Texture> texturetable;

  for(size_t i = 0; i < textures.size(); ++i)
  {
    PackModelPayload::Texture entry;

    entry.type = textures[i].type;
    entry.texture = 1 + meshes.size() + i;

    texturetable.push_back(entry);
  }

  vector<PackModelPayload::Material> materialtable;

  for(size_t i = 0; i < materials.size(); ++i)
  {
    PackModelPayload::Material entry;

    entry.color[0] = materials[i].color.r;
    entry.color[1] = materials[i].color.g;
    entry.color[2] = materials[i].color.b;
    entry.color[3] = 1.0f;
    entry.metalness = 1.0f;
    entry.roughness = 1.0f;
    entry.reflectivity = 0.5f;
    entry.emissive = 0.0f;
    entry.albedomap = materials[i].albedomap;
    entry.surfacemap = materials[i].surfacemap;
    entry.normalmap = materials[i].normalmap;

    materialtable.push_back(entry);
  }

  vector<PackModelPayload::Mesh> meshtable;

  for(size_t i = 0; i < meshes.size(); ++i)
  {
    PackModelPayload::Mesh entry;

    entry.mesh = 1 + i;

    meshtable.push_back(entry);
  }

  vector<PackModelPayload::Instance> instancetable;

  for(size_t i = 0; i < meshes.size(); ++i)
  {
    PackModelPayload::Instance entry;

    entry.mesh = i;
    entry.material = meshes[i].material;
    memcpy(&entry.transform, &meshes[i].transform, sizeof(Transform));
    entry.childcount = 0;

    instancetable.push_back(entry);
  }

  write_modl_asset(fout, id++, texturetable, materialtable, meshtable, instancetable);

  for(auto &mesh : meshes)
  {
    cout << "  Writing: (" << id << ") mesh " << mesh.name << " (" << mesh.vertices.size() << " verts, " << mesh.indices.size()/3 << " faces, " << materials[mesh.material].name << ")" << endl;

    write_mesh_asset(fout, id++, mesh.vertices, mesh.indices);
  }

  for(auto &texture : textures)
  {
    cout << "  Writing: (" << id << ") texture " << texture.paths[0] << endl;

    switch (texture.type)
    {
      case PackModelPayload::Texture::nullmap:
        id++;
        break;

      case PackModelPayload::Texture::albedomap:
        write_diffmap_asset(fout, id++, pathstring(basepath, texture.paths[0]), pathstring(basepath, texture.paths[1]));
        break;

      case PackModelPayload::Texture::surfacemap:
        write_specmap_asset(fout, id++, pathstring(basepath, texture.paths[1]), pathstring(basepath, texture.paths[0]));
        break;

      case PackModelPayload::Texture::normalmap:
        //write_bumpmap_asset(fout, id++, pathstring(basepath, texture.paths[0]));
        write_normalmap_asset(fout, id++, pathstring(basepath, texture.paths[0]));
        break;
    }
  }

  write_chunk(fout, "HEND", 0, nullptr);

  fout.close();

  cout << "Done: " << points.size() << " points, " << normals.size() << " normals, " << texcoords.size() << " texcoords, " << textures.size() - 1 << " textures, " << materials.size() << " materials, " << meshes.size() << " meshes" << endl;
}


void usage()
{
  string msg;

  msg = "Usage: objparser [options] <path>\n";
  msg += "    -scale=<scale>\t\tscale model\n";

  cout << msg << endl;
}


int main(int argc, char **argv)
{
  QGuiApplication app(argc, argv);

  string objfile;

  QStringList args = app.arguments();

  for(int i = 0, j = 0; i < args.size(); ++i)
  {
    if (args[i].startsWith("-"))
      continue;

    if (j == 1)
      objfile = args[i].toStdString();

    ++j;
  }

#ifdef _WIN32
  replace(begin(objfile), end(objfile), '\\', '/');
#endif

  if (objfile == "")
  {
    usage();
    cout << "obj filename missing." << endl;
    exit(1);
  }

  for(int i = 0; i < args.size(); ++i)
  {
    if (args[i].startsWith("-scale="))
      gScale = args[i].split('=')[1].toDouble();
  }

  try
  {
    auto src = pathstring(".", objfile);
    auto dst = pathstring(".", src.basename() + ".pack");

    write_model(src, dst);
  }
  catch(exception &e)
  {
    cerr << "Critical Error: " << e.what() << endl;
  }
}
