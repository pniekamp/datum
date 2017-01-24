//
// Datum - asset dump
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include <QGuiApplication>
#include <cstdio>
#include <fstream>
#include "assetpacker.h"

using namespace std;

namespace
{
  void dump(string const &indent, PackAssetHeader *aset)
  {
    cout << indent << "ASET " << aset->id << '\n';
  }

  void dump(string const &indent, PackCalalogHeader *catl)
  {
    cout << indent << "CATL " << pack_payload_size(*catl) << " bytes" << '\n';
  }

  void dump(string const &indent, PackTextHeader *text)
  {
    cout << indent << "TEXT " << pack_payload_size(*text) << " bytes" << '\n';
  }

  void dump(string const &indent, PackImageHeader *imag)
  {
    cout << indent << "IMAG " << imag->width << "x" << imag->height << "x" << imag->layers << "x" << imag->levels << ":" << imag->format << " " << pack_payload_size(*imag) << " bytes" << '\n';
  }

  void dump(string const &indent, PackFontHeader *font)
  {
    cout << indent << "FONT " << font->glyphcount << " glyphs " << pack_payload_size(*font) << " bytes" << '\n';
  }

  void dump(string const &indent, PackMeshHeader *mesh)
  {
    cout << indent << "MESH " << mesh->vertexcount << " vertices " << mesh->indexcount << " indices " << pack_payload_size(*mesh) << " bytes" << '\n';
  }

  void dump(string const &indent, PackMaterialHeader *matl)
  {
    cout << indent << "MATL " << pack_payload_size(*matl) << " bytes" << '\n';
  }

  void dump(string const &indent, PackModelHeader *modl)
  {
    cout << indent << "MODL " << modl->texturecount << " textures " << modl->materialcount << " materials " << modl->meshcount << " meshes " << modl->instancecount << " instances " << pack_payload_size(*modl) << " bytes" << '\n';
  }

  void dump(string const &indent, PackParticleSystemHeader *ptsm)
  {
    cout << indent << "PTSM " << ptsm->emittercount << " emitters " << pack_payload_size(*ptsm) << " bytes" << '\n';
  }
}

///////////////////////// dump //////////////////////////////////////////
void dump(const char *path)
{
  ifstream fin(path, ios::binary);

  fin.seekg(sizeof(PackHeader), ios::beg);

  string indent = "  ";

  PackChunk chunk;

  while (fin.read((char*)&chunk, sizeof(chunk)))
  {
    vector<char> buffer(chunk.length);

    fin.read(buffer.data(), chunk.length);

    switch (chunk.type)
    {
      case "ASET"_packchunktype:
        dump(indent, reinterpret_cast<PackAssetHeader*>(buffer.data()));
        indent += "  ";
        break;

      case "CATL"_packchunktype:
        dump(indent, reinterpret_cast<PackCalalogHeader*>(buffer.data()));
        break;

      case "TEXT"_packchunktype:
        dump(indent, reinterpret_cast<PackTextHeader*>(buffer.data()));
        break;

      case "IMAG"_packchunktype:
        dump(indent, reinterpret_cast<PackImageHeader*>(buffer.data()));
        break;

      case "FONT"_packchunktype:
        dump(indent, reinterpret_cast<PackFontHeader*>(buffer.data()));
        break;

      case "MESH"_packchunktype:
        dump(indent, reinterpret_cast<PackMeshHeader*>(buffer.data()));
        break;

      case "MATL"_packchunktype:
        dump(indent, reinterpret_cast<PackMaterialHeader*>(buffer.data()));
        break;

      case "MODL"_packchunktype:
        dump(indent, reinterpret_cast<PackModelHeader*>(buffer.data()));
        break;

      case "PTSM"_packchunktype:
        dump(indent, reinterpret_cast<PackParticleSystemHeader*>(buffer.data()));
        break;

      case "AEND"_packchunktype:
        indent = indent.substr(0, indent.size()-2);
        break;

      case "DATA"_packchunktype:
        cout << indent << "DATA\n";
        break;

      case "CDAT"_packchunktype:
        cout << indent << "CDAT\n";
        break;

      case "HEND"_packchunktype:
        indent = indent.substr(0, indent.size()-2);
        cout << indent << "HEND\n";
        break;

      default:
        throw runtime_error("Unhandled Pack Chunk");
    }

    fin.seekg(sizeof(uint32_t), ios::cur);
  }

  fin.close();
}


///////////////////////// main //////////////////////////////////////////////
int main(int argc, char **argv)
{
  QGuiApplication app(argc, argv);

  cout << "Asset dump" << endl;

  if (argc < 2)
  {
    cout << "Pack filename missing" << endl;
    exit(1);
  }

  try
  {
    for(int i = 1; i < argc; ++i)
    {
      cout << "Dumping: " << argv[i] << endl;

      dump(argv[i]);
    }
  }
  catch(exception &e)
  {
    cerr << "Critical Error: " << e.what() << endl;
  }
}
