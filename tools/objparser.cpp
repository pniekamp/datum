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
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <leap.h>
#include <leap/pathstring.h>
#include "assetpacker.h"

using namespace std;
using namespace lml;
using namespace leap;

float gScale = 1.0f;

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

      auto tangent = tan1[i];
      auto binormal = tan2[i];

      orthonormalise(normal, tangent, binormal);

      vertices[i].tangent[0] = tangent.x;
      vertices[i].tangent[1] = tangent.y;
      vertices[i].tangent[2] = tangent.z;
      vertices[i].tangent[3] = (dot(binormal, tan2[i]) < 0.0f) ? -1.0f : 1.0f;
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

  vector<char> payload(sizeof(PackImagePayload) + image_datasize(width, height, layers, levels));

  memcpy(payload.data() + sizeof(PackImagePayload), image.bits(), image.byteCount());

  image_buildmips_srgb_a(0.5, width, height, layers, levels, payload.data());

  image_compress_bc3(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, payload.data(), 0.0f, 0.0f);

  return id + 1;
}


uint32_t write_specmap_asset(ostream &fout, uint32_t id, string const &path, string const &base = "")
{
  QImage image(path.c_str());

  if (image.isNull())
    throw runtime_error("Failed to load image - " + path);

  image = image.convertToFormat(QImage::Format_ARGB32);

  if (base != "")
  {
    QImage albedo(base.c_str());

    if (albedo.isNull())
      throw runtime_error("Failed to load image - " + base);

    if (albedo.size() != image.size())
      throw runtime_error("Image size mismatch - " + base);

    for(int y = 0; y < image.height(); ++y)
    {
      for(int x = 0; x < image.width(); ++x)
      {
        auto color = rgba(albedo.pixel(x, y));
        auto intensity = rgba(image.pixel(x, y));
        float shininess = qAlpha(image.pixel(x, y))/255.0;;

        image.setPixel(x, y, qRgba(color.r * intensity.r * 255, color.g * intensity.g * 255, color.b * intensity.b * 255, shininess * 255));
      }
    }
  }

  image = image.mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(sizeof(PackImagePayload) + image_datasize(width, height, layers, levels));

  memcpy(payload.data() + sizeof(PackImagePayload), image.bits(), image.byteCount());

  image_buildmips_srgb(width, height, layers, levels, payload.data());

  image_compress_bc3(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, payload.data(), 0.0f, 0.0f);

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

      Vec3 normal = normalise(Vec3((s[2] + 2*s[5] + s[8]) - (s[0] + 2*s[3] + s[6]), (s[6] + 2*s[7] + s[8]) - (s[0] + 2*s[1] + s[2]), 1/strength));

      image.setPixel(x, y, qRgba((0.5f*normal.x+0.5f)*255, (0.5f*normal.y+0.5f)*255, (0.5f*normal.z+0.5f)*255, 255));
    }
  }

  image = image.mirrored();

  int width = image.width();
  int height = image.height();
  int layers = 1;
  int levels = min(4, image_maxlevels(width, height));

  vector<char> payload(sizeof(PackImagePayload) + image_datasize(width, height, layers, levels));

  memcpy(payload.data() + sizeof(PackImagePayload), image.bits(), image.byteCount());

  image_buildmips_rgb(width, height, layers, levels, payload.data());

  write_imag_asset(fout, id, width, height, layers, levels, payload.data(), 0.0f, 0.0f);

  return id + 1;
}


void write_model(string const &filename)
{
  string mtllib;

  vector<Vec3> points;
  vector<Vec3> normals;
  vector<Vec2> texcoords;

  struct Texture
  {
    int type;
    string path;
    string base;
  };

  vector<Texture> textures;

  textures.push_back({ PackModelPayload::Texture::defaulttexture, "default" });

  struct Material
  {
    string name;

    Color3 color = Color3(1.0f, 1.0f, 1.0f);

    Color3 specularintensity = Color3(0.5f, 0.5f, 0.5f);
    float specularexponent = 96.0f;

    uint32_t albedomap = 0;
    uint32_t specularmap = 0;
    uint32_t normalmap = 0;

    float disolve = 1.0f;

    string albedobase;
    string albedomask;
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

  ifstream fin(filename);

  if (!fin)
    throw runtime_error("unable to read obj file - " + filename);

  cout << "  Reading: " << filename << endl;

  string buffer;

  while (getline(fin, buffer))
  {
    buffer = trim(buffer);

    // skip comments
    if (buffer.empty() || buffer[0] == '#' || buffer[0] == '/')
      continue;

    auto fields = split(buffer);

    if (fields[0] == "mtllib")
    {
      mtllib = pathstring(pathstring(filename).base(), fields[1]);
    }

    if (fields[0] == "usemtl")
    {
      meshes.push_back(Mesh());

      mesh = &meshes.back();

      mesh->name = name;
      mesh->usemtl = fields[1];
      mesh->transform = transform;
    }

    if (fields[0] == "o" || fields[0] == "g")
    {
      name = fields[1];
      transform = Transform::identity();
    }

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
      if (!mesh)
        throw runtime_error("Object not specified");

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

        if (mesh->vertexmap.find(vertex) == mesh->vertexmap.end())
        {
          mesh->vertices.push_back(vertex);

          mesh->vertexmap[vertex] = mesh->vertices.size() - 1;
        }

        mesh->indices.push_back(mesh->vertexmap[vertex]);
      }
    }
  }

  cout << "  Parsing: " << filename << " (" << points.size() << " points, " << texcoords.size() << " texcoords, " << points.size() << " normals)" << endl;

  if (mtllib != "")
  {
    ifstream fin(mtllib);

    if (!fin)
      throw runtime_error("unable to read mtl file - " + mtllib);

    cout << "  Reading: " << mtllib << endl;

    string buffer;

    while (getline(fin, buffer))
    {
      buffer = trim(buffer);

      // skip comments
      if (buffer.empty() || buffer[0] == '#' || buffer[0] == '/')
        continue;

      auto fields = split(buffer);

      if (fields[0] == "newmtl")
      {
        materials.push_back({ fields[1] });

        material = &materials.back();

        for(auto &mesh : meshes)
        {
          if (mesh.usemtl == fields[1])
            mesh.material = materials.size() - 1;
        }
      }

      if (fields[0] == "Kd")
      {
        material->color[0] = ato<float>(fields[1]);
        material->color[1] = ato<float>(fields[2]);
        material->color[2] = ato<float>(fields[3]);
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

      if (fields[0] == "map_Kd")
      {
        material->albedobase = fields[1];

        if (material->specularmap != 0)
          textures[material->specularmap].base = material->albedobase;
      }

      if (fields[0] == "map_d")
      {
        material->albedomask = fields[1];

        if (material->albedomap != 0)
          textures[material->albedomap].base = material->albedomask;
      }

      if (fields[0] == "map_Kd")
      {
        auto j = find_if(textures.begin(), textures.end(), [&](auto &texture) { return (texture.type == PackModelPayload::Texture::albedomap && texture.path == fields[1]); });

        if (j == textures.end())
        {
          textures.push_back({ PackModelPayload::Texture::albedomap, fields[1], material->albedomask });
          j = textures.end() - 1;
        }

        material->albedomap = j - textures.begin();

        if (norm(material->color) < 0.01)
          material->color = Color3(1, 1, 1);
      }

      if (fields[0] == "d")
      {
        material->disolve = ato<float>(fields[1]);
      }

      if (fields[0] == "map_Ks")
      {
        textures.push_back({ PackModelPayload::Texture::specularmap, fields[1], material->albedobase });

        material->specularmap = textures.size() - 1;
      }

      if (fields[0] == "map_Bump" || fields[0] == "map_bump" || fields[0] == "bump")
      {
        auto j = find_if(textures.begin(), textures.end(), [&](auto &texture) { return (texture.type == PackModelPayload::Texture::normalmap && texture.path == fields[1]); });

        if (j == textures.end())
        {
          textures.push_back({ PackModelPayload::Texture::normalmap, fields[1] });
          j = textures.end() - 1;
        }

        material->normalmap = j - textures.begin();
      }
    }

    cout << "  Parsing: " << mtllib << " (" << materials.size()-1 << " materials, " << textures.size()-1 << " textures)" << endl;
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

    calculatetangents(mesh.vertices, mesh.indices);
  }

  auto batchorder = [&](Mesh const &lhs) { return make_tuple(materials[lhs.material].albedomap, materials[lhs.material].normalmap); };

  sort(meshes.begin(), meshes.end(), [&](auto &lhs, auto &rhs) { return batchorder(lhs) < batchorder(rhs); });


  //
  // Output
  //

  uint32_t id = 0;
  string output = (QFileInfo(filename.c_str()).baseName() + ".pack").toStdString();

  ofstream fout(output, ios::binary | ios::trunc);

  write_header(fout);

  cout << "  Writing: (" << id << ") model " << output << endl;

  PackAssetHeader aset = { id++ };

  write_chunk(fout, "ASET", sizeof(aset), &aset);

  PackModelHeader modl = { (uint32_t)textures.size(), (uint32_t)materials.size(), (uint32_t)meshes.size(), (uint32_t)meshes.size(), (size_t)fout.tellp() + sizeof(modl) + sizeof(PackChunk) + sizeof(uint32_t) };

  write_chunk(fout, "MODL", sizeof(modl), &modl);

  size_t datasize = modl.texturecount*sizeof(PackModelPayload::Texture) + modl.materialcount*sizeof(PackModelPayload::Material) + modl.meshcount*sizeof(PackModelPayload::Mesh) + modl.instancecount*sizeof(PackModelPayload::Instance);

  vector<char> payload(datasize);

  auto texturetable = reinterpret_cast<PackModelPayload::Texture*>(payload.data());
  auto materialtable = reinterpret_cast<PackModelPayload::Material*>(payload.data() + textures.size()*sizeof(PackModelPayload::Texture));
  auto meshtable = reinterpret_cast<PackModelPayload::Mesh*>(payload.data() + textures.size()*sizeof(PackModelPayload::Texture) + materials.size()*sizeof(PackModelPayload::Material));
  auto instancetable = reinterpret_cast<PackModelPayload::Instance*>(payload.data() + textures.size()*sizeof(PackModelPayload::Texture) + materials.size()*sizeof(PackModelPayload::Material) + meshes.size()*sizeof(PackModelPayload::Mesh));

  for(size_t i = 0; i < textures.size(); ++i)
  {
    texturetable[i].type = textures[i].type;
    texturetable[i].texture = 1 + meshes.size() + i;
  }

  for(size_t i = 0; i < materials.size(); ++i)
  {
    materialtable[i].color[0] = materials[i].color.r;
    materialtable[i].color[1] = materials[i].color.g;
    materialtable[i].color[2] = materials[i].color.b;
    materialtable[i].metalness = 0.0f;
    materialtable[i].smoothness = 0.0f;
    materialtable[i].reflectivity = 0.5f;
    materialtable[i].albedomap = materials[i].albedomap;
    materialtable[i].specularmap = 0;
    materialtable[i].normalmap = materials[i].normalmap;
  }

  for(size_t i = 0; i < meshes.size(); ++i)
  {
    meshtable[i].mesh = 1 + i;

    instancetable[i].mesh = i;
    instancetable[i].material = meshes[i].material;
    memcpy(&instancetable[i].transform, &meshes[i].transform, sizeof(Transform));

    instancetable[i].childcount = 0;
  }

  write_chunk(fout, "DATA", payload.size(), payload.data());

  write_chunk(fout, "AEND", 0, nullptr);

  for(auto &mesh : meshes)
  {
    cout << "  Writing: (" << id << ") mesh " << mesh.name << " (" << mesh.vertices.size() << " verts, " << mesh.indices.size()/3 << " faces, " << materials[mesh.material].name << ")" << endl;

    write_mesh_asset(fout, id++, mesh.vertices, mesh.indices);
  }

  for(auto &texture: textures)
  {
    cout << "  Writing: (" << id << ") texture " << texture.path << endl;

    switch (texture.type)
    {
      case PackModelPayload::Texture::defaulttexture:
        id++;
        break;

      case PackModelPayload::Texture::albedomap:
        write_diffmap_asset(fout, id++, pathstring(pathstring(filename).base(), texture.path), pathstring(pathstring(filename).base(), texture.base));
        break;

      case PackModelPayload::Texture::specularmap:
        write_specmap_asset(fout, id++, pathstring(pathstring(filename).base(), texture.path), pathstring(pathstring(filename).base(), texture.base));
        break;

      case PackModelPayload::Texture::normalmap:
        write_bumpmap_asset(fout, id++, pathstring(pathstring(filename).base(), texture.path));
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
    write_model(objfile);
  }
  catch(std::exception &e)
  {
    cerr << "Critical Error: " << e.what() << endl;
  }
}
