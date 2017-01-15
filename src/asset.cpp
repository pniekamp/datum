//
// Datum - asset system
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "asset.h"
#include "assetpack.h"
#include <leap/lz4.h>
#include "debug.h"
#include <algorithm>

using namespace std;
using namespace leap::crypto;

//|---------------------- AssetManager --------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// AssetManager::Constructor /////////////////////////
AssetManager::AssetManager(allocator_type const &allocator)
  : m_allocator(allocator),
    m_files(allocator),
    m_assets(allocator)
{
  m_head = nullptr;

#ifdef DEBUG
  barriercount = 0;
#endif
}


///////////////////////// AssetManager::initialise //////////////////////////
void AssetManager::initialise(size_t slotcount, size_t slabsize)
{
  m_files.reserve(256);
  m_assets.reserve(slotcount);

  m_head = new(allocate<char>(m_allocator, slabsize, alignof(Slot))) Slot;

  m_head->size = slabsize;
  m_head->after = nullptr;
  m_head->prev = m_head;
  m_head->next = m_head;
  m_head->state = Slot::State::Empty;

  cout << "Asset Storage: " << slotcount / 1024 << "k, " << slabsize / 1024 / 1024 << " MiB" << endl;
}


///////////////////////// AssetManager::load ////////////////////////////////
Asset const *AssetManager::load(DatumPlatform::v1::PlatformInterface &platform, const char *identifier)
{
  leap::threadlib::SyncLock lock(m_mutex);

  int count = 0;

  try
  {
    File file;

    file.baseid = m_assets.size();
    file.handle = platform.open_handle(identifier);

    m_files.push_back(file);

    AssetEx asset;

    asset.slot = nullptr;
    asset.file = &m_files.back();

    PackHeader header;

    platform.read_handle(file.handle, 0, &header, sizeof(header));

    if (header.signature[0] != 0xD9 || header.signature[1] != 'S' || header.signature[2] != 'V' || header.signature[3] != 'A')
      throw runtime_error("Invalid sva file");

    uint64_t position = sizeof(header);

    while (true)
    {
      PackChunk chunk;

      platform.read_handle(file.handle, position, &chunk, sizeof(chunk));

      if (chunk.type == "HEND"_packchunktype)
        break;

      switch (chunk.type)
      {
        case "ASET"_packchunktype:
          {
            PackAssetHeader aset;

            platform.read_handle(file.handle, position + sizeof(chunk), &aset, sizeof(aset));

            asset.id = file.baseid + aset.id;

            if (asset.id + 1 >= m_assets.capacity())
              throw runtime_error("Asset Count Exhausted");

            break;
          }

        case "CATL"_packchunktype:
          {
            PackCalalogHeader catl;

            platform.read_handle(file.handle, position + sizeof(chunk), &catl, sizeof(catl));

            asset.magic = catl.magic;
            asset.version = catl.version;
            asset.datasize = pack_payload_size(catl);
            asset.datapos = catl.dataoffset;

            break;
          }

        case "TEXT"_packchunktype:
          {
            PackTextHeader text;

            platform.read_handle(file.handle, position + sizeof(chunk), &text, sizeof(text));

            asset.length = text.length;
            asset.datasize = pack_payload_size(text);
            asset.datapos = text.dataoffset;

            break;
          }

        case "IMAG"_packchunktype:
          {
            PackImageHeader imag;

            platform.read_handle(file.handle, position + sizeof(chunk), &imag, sizeof(imag));

            asset.width = imag.width;
            asset.height = imag.height;
            asset.layers = imag.layers;
            asset.levels = imag.levels;
            asset.format = imag.format;
            asset.datasize = pack_payload_size(imag);
            asset.datapos = imag.dataoffset;

            break;
          }

        case "FONT"_packchunktype:
          {
            PackFontHeader font;

            platform.read_handle(file.handle, position + sizeof(chunk), &font, sizeof(font));

            asset.ascent = font.ascent;
            asset.descent = font.descent;
            asset.leading = font.leading;
            asset.glyphcount = font.glyphcount;
            asset.datasize = pack_payload_size(font);
            asset.datapos = font.dataoffset;

            break;
          }

        case "MESH"_packchunktype:
          {
            PackMeshHeader mesh;

            platform.read_handle(file.handle, position + sizeof(chunk), &mesh, sizeof(mesh));

            asset.vertexcount = mesh.vertexcount;
            asset.indexcount = mesh.indexcount;
            asset.mincorner[0] = mesh.mincorner[0];
            asset.mincorner[1] = mesh.mincorner[1];
            asset.mincorner[2] = mesh.mincorner[2];
            asset.maxcorner[0] = mesh.maxcorner[0];
            asset.maxcorner[1] = mesh.maxcorner[1];
            asset.maxcorner[2] = mesh.maxcorner[2];
            asset.datasize = pack_payload_size(mesh);
            asset.datapos = mesh.dataoffset;

            break;
          }

        case "MATL"_packchunktype:
          {
            PackMaterialHeader matl;

            platform.read_handle(file.handle, position + sizeof(chunk), &matl, sizeof(matl));

            asset.datasize = pack_payload_size(matl);
            asset.datapos = matl.dataoffset;

            break;
          }

        case "MODL"_packchunktype:
          {
            PackModelHeader modl;

            platform.read_handle(file.handle, position + sizeof(chunk), &modl, sizeof(modl));

            asset.texturecount = modl.texturecount;
            asset.materialcount = modl.materialcount;
            asset.meshcount = modl.meshcount;
            asset.instancecount = modl.instancecount;
            asset.datasize = pack_payload_size(modl);
            asset.datapos = modl.dataoffset;

            break;
          }

        case "PCSM"_packchunktype:
          {
            PackParticleSystemHeader pcsm;

            platform.read_handle(file.handle, position + sizeof(chunk), &pcsm, sizeof(pcsm));

            asset.minrange[0] = pcsm.minrange[0];
            asset.minrange[1] = pcsm.minrange[1];
            asset.minrange[2] = pcsm.minrange[2];
            asset.maxrange[0] = pcsm.maxrange[0];
            asset.maxrange[1] = pcsm.maxrange[1];
            asset.maxrange[2] = pcsm.maxrange[2];
            asset.maxparticles = pcsm.maxparticles;
            asset.emittercount = pcsm.emittercount;
            asset.datasize = pack_payload_size(pcsm);
            asset.datapos = pcsm.dataoffset;

            break;
          }

        case "AEND"_packchunktype:
          {
            m_assets.resize(max(asset.id + 1, m_assets.size()));

            m_assets[asset.id] = asset;

            ++count;

            break;
          }

        case "DATA"_packchunktype:
        case "CDAT"_packchunktype:
          break;

        default:
          cout << "Unhandled Pack Chunk" << endl;
          break;

      }

      position += chunk.length + sizeof(chunk) + sizeof(uint32_t);
    }

    cout << "Asset Pack Loaded: " << identifier << " (" << count << " assets)" << endl;

    return &m_assets[file.baseid];
  }
  catch(exception &e)
  {
    cerr << "Asset Pack Load Error: " << e.what() << endl;
  }

  return nullptr;
}


///////////////////////// AssetManager::request /////////////////////////////
Asset const *AssetManager::find(size_t id) const
{
  leap::threadlib::SyncLock lock(m_mutex);

  if (id >= m_assets.size())
    return nullptr;

  return &m_assets[id];
}


///////////////////////// AssetManager::acquire_slot ////////////////////////
AssetManager::Slot *AssetManager::acquire_slot(size_t size)
{
  auto bytes = ((size + sizeof(Slot) - 1)/alignof(Slot) + 1) * alignof(Slot);

  for(auto slot = m_head; true; slot = slot->next)
  {
    if (slot->state == Slot::State::Barrier)
      return nullptr;

    if (slot->state == Slot::State::Loaded)
    {
      // evict

      slot->asset->slot = nullptr;

      slot->state = Slot::State::Empty;
    }

    if (slot->state == Slot::State::Empty)
    {
      if (slot->after && slot->after->state == Slot::State::Empty)
      {
        // merge

        slot->after->prev->next = slot->after->next;
        slot->after->next->prev = slot->after->prev;

        slot->size += slot->after->size;
        slot->after = slot->after->after;

        m_head = slot;
      }

      if (slot->size > bytes + sizeof(Slot))
      {
        // split

        Slot *newslot = new(reinterpret_cast<char*>(slot) + bytes) Slot;

        newslot->size = slot->size - bytes;
        newslot->after = slot->after;
        newslot->prev = m_head->prev;
        newslot->next = m_head;
        newslot->state = Slot::State::Empty;

        newslot->prev->next = newslot;
        newslot->next->prev = newslot;

        slot->size = bytes;
        slot->after = newslot;

        m_head = newslot;
      }

      if (slot->size >= bytes)
      {
        // match

        touch_slot(slot);

        return slot;
      }
    }

    if (slot->next == m_head)
      return nullptr;
  }
}


///////////////////////// AssetManager::touch_slot //////////////////////////
AssetManager::Slot *AssetManager::touch_slot(AssetManager::Slot *slot)
{
  if (slot == m_head)
    m_head = m_head->next;

  slot->prev->next = slot->next;
  slot->next->prev = slot->prev;

  slot->next = m_head;
  slot->prev = m_head->prev;

  slot->prev->next = slot;
  slot->next->prev = slot;

  return slot;
}


///////////////////////// AssetManager::request /////////////////////////////
void const *AssetManager::request(DatumPlatform::PlatformInterface &platform, Asset const *asset)
{
  assert(asset);

  leap::threadlib::SyncLock lock(m_mutex);

  auto &assetex = m_assets[static_cast<AssetEx const *>(asset) - m_assets.data()];

  auto &slot = assetex.slot;

  if (!slot)
  {
    slot = acquire_slot(assetex.datasize);

    if (slot)
    {
      slot->state = Slot::State::Loading;

      slot->asset = &assetex;

      platform.submit_work(background_loader, this, slot);
    }

    return nullptr;
  }

  touch_slot(slot);

  if (slot->state != Slot::State::Loaded)
    return nullptr;

  return slot->data;
}


///////////////////////// AssetManager::acquire_barrier /////////////////////
uintptr_t AssetManager::acquire_barrier()
{
  leap::threadlib::SyncLock lock(m_mutex);

  auto slot = acquire_slot(0);

  if (slot)
  {
    slot->state = Slot::State::Barrier;
  }

#ifdef DEBUG
  ++barriercount;
#endif

  return reinterpret_cast<uintptr_t>(slot);
}


///////////////////////// AssetManager::release_barrier /////////////////////
void AssetManager::release_barrier(uintptr_t barrier)
{
  leap::threadlib::SyncLock lock(m_mutex);

  auto slot = reinterpret_cast<Slot*>(barrier);

  if (slot)
  {
    slot->state = Slot::State::Empty;
  }

#ifdef DEBUG
  --barriercount;
#endif
}


///////////////////////// AssetManager::background_loader ///////////////////
void AssetManager::background_loader(DatumPlatform::PlatformInterface &platform, void *ldata, void *rdata)
{
  BEGIN_TIMED_BLOCK(Asset, lml::Color3(0.2f, 1.0f, 0.4f))

  auto &manager = *static_cast<AssetManager*>(ldata);

  auto &slot = *static_cast<Slot*>(rdata);

  try
  {
    if (slot.asset->file == nullptr)
      throw runtime_error("Invalid File Handle");

    PackChunk chunk;

    platform.read_handle(slot.asset->file->handle, slot.asset->datapos, &chunk, sizeof(PackChunk));

    switch (chunk.type)
    {
      case "DATA"_packchunktype:
        {
          if (chunk.length != slot.asset->datasize)
            throw runtime_error("Chunk Data Size Mismatch");

          platform.read_handle(slot.asset->file->handle, slot.asset->datapos + sizeof(PackChunk), slot.data, slot.asset->datasize);

          break;
        }

      case "CDAT"_packchunktype:
        {
          size_t remaining = chunk.length;
          uint8_t *datapos = slot.data;
          uint64_t filepos = slot.asset->datapos + sizeof(PackChunk);

          while (remaining != 0)
          {
            PackBlock block;

            auto bytes = min(sizeof(block), remaining);

            platform.read_handle(slot.asset->file->handle, filepos, &block, bytes);

            datapos += lz4_decompress(block.data, datapos, block.size, slot.data + slot.asset->datasize - datapos);

            filepos += bytes;
            remaining -= bytes;
          }

          break;
        }

      default:
        throw runtime_error("Unhandled Pack Data Chunk");
    }

    {
      leap::threadlib::SyncLock lock(manager.m_mutex);

      slot.state = Slot::State::Loaded;
    }
  }
  catch(exception &e)
  {
    cerr << e.what() << endl;
  }

  END_TIMED_BLOCK(Asset)
}


///////////////////////// initialise_asset_system ///////////////////////////
bool initialise_asset_system(DatumPlatform::PlatformInterface &platform, AssetManager &assetmanager, size_t slotcount, size_t slabsize)
{
  assetmanager.initialise(slotcount, slabsize);

  return true;
}
