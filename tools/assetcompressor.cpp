//
// Datum - asset compressor
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include <QGuiApplication>
#include <cstdio>
#include <fstream>
#include "assetpacker.h"

using namespace std;

///////////////////////// compress //////////////////////////////////////////
void compress(const char *path)
{
  ifstream fin(path, ios::binary);

  fstream fout("tmp.pack", ios::in | ios ::out | ios::binary | ios::trunc);

  write_header(fout);

  // First Pass, add asset metadata

  fin.seekg(sizeof(PackHeader), ios::beg);

  while (fin)
  {
    PackChunk chunk;

    fin.read((char*)&chunk, sizeof(chunk));

    if (chunk.type == "HEND"_packchunktype)
      break;

    switch (chunk.type)
    {
      case "ASET"_packchunktype:
      case "CATL"_packchunktype:
      case "TEXT"_packchunktype:
      case "FONT"_packchunktype:
      case "IMAG"_packchunktype:
      case "MESH"_packchunktype:
      case "MATL"_packchunktype:
      case "MODL"_packchunktype:
      case "AEND"_packchunktype:
        {
          std::vector<char> buffer(chunk.length);

          fin.read(buffer.data(), chunk.length);

          write_chunk(fout, (char*)&chunk.type, buffer.size(), buffer.data());

          break;
        }

      case "DATA"_packchunktype:
      case "CDAT"_packchunktype:
        fin.seekg(chunk.length, ios::cur);
        break;

      default:
        throw runtime_error("Unhandled Pack Chunk");
    }

    fin.seekg(sizeof(uint32_t), ios::cur);
  }

  write_chunk(fout, "HEND", 0, nullptr);

  // Second Pass, add compressed data

  fout.seekg(sizeof(PackHeader), ios::beg);

  while (fout)
  {
    PackChunk chunk;

    fout.read((char*)&chunk, sizeof(chunk));

    if (chunk.type == "HEND"_packchunktype)
      break;

    auto position = fout.tellg();

    switch (chunk.type)
    {
//      case "CATL"_packchunktype:
      case "TEXT"_packchunktype:
      case "IMAG"_packchunktype:
      case "FONT"_packchunktype:
      case "MESH"_packchunktype:
      case "MATL"_packchunktype:
      case "MODL"_packchunktype:
        {
          uint64_t dataoffset;

          fout.seekg((size_t)position + chunk.length - sizeof(uint64_t), ios::beg);
          fout.read((char*)&dataoffset, sizeof(dataoffset));

          // write compressed dat

          PackChunk dat;

          fin.seekg(dataoffset);

          fin.read((char*)&dat, sizeof(dat));

          std::vector<char> buffer(dat.length);

          fin.read(buffer.data(), dat.length);

          fout.seekp(0, ios::end);

          dataoffset = fout.tellp();

          switch (dat.type)
          {
            case "DATA"_packchunktype:
              write_compressed_chunk(fout, "CDAT", buffer.size(), buffer.data());
              break;

            case "CDAT"_packchunktype:
              write_chunk(fout, "CDAT", buffer.size(), buffer.data());
              break;

            default:
              throw runtime_error("Unhandled Pack Data Chunk");
          }

          // rewrite hdr

          fout.seekg((size_t)position + chunk.length - sizeof(uint64_t), ios::beg);
          fout.write((char*)&dataoffset, sizeof(dataoffset));
        }
    }

    fout.seekg(position, ios::beg);
    fout.seekg(chunk.length + sizeof(uint32_t), ios::cur);
  }

  fin.close();
  fout.close();

  remove(path);
  rename("tmp.pack", path);
}


///////////////////////// main //////////////////////////////////////////////
int main(int argc, char **argv)
{
  QGuiApplication app(argc, argv);

  cout << "Asset Compressor" << endl;

  if (argc < 2)
  {
    cout << "Pack filename missing" << endl;
    exit(1);
  }

  try
  {
    for(int i = 1; i < argc; ++i)
    {
      cout << "  Compressing: " << argv[i] << endl;

      compress(argv[i]);
    }
  }
  catch(exception &e)
  {
    cerr << "Critical Error: " << e.what() << endl;
  }
}
