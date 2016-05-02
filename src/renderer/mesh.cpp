//
// Datum - mesh
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "mesh.h"
#include "resource.h"
#include "assetpack.h"
#include "debug.h"

using namespace std;
using namespace lml;

namespace
{
  size_t mesh_datasize(int vertexcount, int indexcount)
  {
    assert(vertexcount > 0 && indexcount > 0);

    return vertexcount * sizeof(Vertex) + 4096 + indexcount * sizeof(uint32_t);
  }
}

//|---------------------- Mesh ----------------------------------------------
//|--------------------------------------------------------------------------


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Mesh const *ResourceManager::create<Mesh>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(Mesh));

  if (!slot)
    return nullptr;

  auto mesh = new(slot) Mesh;

  mesh->bound = Bound3(Vec3(asset->mincorner[0], asset->mincorner[1], asset->mincorner[2]), Vec3(asset->maxcorner[0], asset->maxcorner[1], asset->maxcorner[2]));
  mesh->vertices = nullptr;
  mesh->indices = nullptr;
  mesh->transferlump = nullptr;
  mesh->state = Mesh::State::Empty;

  set_slothandle(slot, asset);

  return mesh;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Mesh const *ResourceManager::create<Mesh>(int vertexcount, int indexcount)
{
  auto slot = acquire_slot(sizeof(Mesh));

  if (!slot)
    return nullptr;

  auto lump = acquire_lump(mesh_datasize(vertexcount, indexcount));

  if (!lump)
    return nullptr;

  auto mesh = new(slot) Mesh;

  mesh->transferlump = lump;

  wait(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  mesh->vertexbuffer = create_vertexbuffer(vulkan, lump->commandbuffer, vertexcount, sizeof(Vertex), indexcount, sizeof(uint32_t));

  end(vulkan, lump->commandbuffer);

  submit_transfer(lump);

  mesh->vertices = (Vertex*)((uint8_t*)lump->transfermemory + mesh->vertexbuffer.verticiesoffset);
  mesh->indices = (uint32_t*)((uint8_t*)lump->transfermemory + mesh->vertexbuffer.indicesoffset);

  mesh->state = Mesh::State::Ready;

  set_slothandle(slot, nullptr);

  return mesh;
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Mesh>(Mesh const *mesh)
{
  assert(mesh);
  assert(mesh->vertices && mesh->indices);
  assert(mesh->state == Mesh::State::Ready);

  auto slot = const_cast<Mesh*>(mesh);

  auto lump = slot->transferlump;

  wait(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  update_vertexbuffer(lump->commandbuffer, lump->transferbuffer, slot->vertexbuffer);

  end(vulkan, lump->commandbuffer);

  submit_transfer(lump);

  while (!test(vulkan, lump->fence))
    ;
}

template<>
void ResourceManager::update<Mesh>(Mesh const *mesh, Vertex const *vertices, uint32_t const *indices)
{
  assert(mesh);
  assert(mesh->vertices && mesh->indices);
  assert(mesh->state == Mesh::State::Ready);

  auto slot = const_cast<Mesh*>(mesh);

  memcpy(slot->vertices, vertices, mesh->vertexbuffer.vertexcount * mesh->vertexbuffer.vertexsize);
  memcpy(slot->indices, indices, mesh->vertexbuffer.indexcount * mesh->vertexbuffer.indexsize);

  update(mesh);
}

template<>
void ResourceManager::update<Mesh>(Mesh const *mesh, Vertex *vertices, uint32_t *indices)
{
  update<Mesh>(mesh, (Vertex const *)vertices, (uint32_t const *)indices);
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Mesh>(Mesh const *mesh, Vertex const *vertices)
{
  assert(mesh);
  assert(mesh->vertices);
  assert(mesh->state == Mesh::State::Ready);

  auto slot = const_cast<Mesh*>(mesh);

  memcpy(slot->vertices, vertices, mesh->vertexbuffer.vertexcount * mesh->vertexbuffer.vertexsize);

  update(mesh);
}

template<>
void ResourceManager::update<Mesh>(Mesh const *mesh, Vertex *vertices)
{
  update<Mesh>(mesh, (Vertex const *)vertices);
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Mesh>(Mesh const *mesh, Bound3 bound)
{
  assert(mesh);

  auto slot = const_cast<Mesh*>(mesh);

  slot->bound = bound;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<Mesh>(DatumPlatform::PlatformInterface &platform, Mesh const *mesh)
{
  assert(mesh);

  auto slot = const_cast<Mesh*>(mesh);

  Mesh::State empty = Mesh::State::Empty;

  if (slot->state.compare_exchange_strong(empty, Mesh::State::Loading))
  {
    auto asset = get_slothandle<Asset const *>(slot);

    if (asset)
    {
      auto bits = m_assets->request(platform, asset);

      if (bits)
      {
        auto vertextable = reinterpret_cast<Vertex const *>((size_t)bits);
        auto indextable = reinterpret_cast<uint32_t const *>((size_t)bits + asset->vertexcount*sizeof(Vertex));

        auto lump = acquire_lump(mesh_datasize(asset->vertexcount, asset->indexcount));

        if (lump)
        {
          slot->transferlump = lump;

          wait(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          slot->vertexbuffer = create_vertexbuffer(vulkan, lump->commandbuffer, asset->vertexcount, sizeof(Vertex), asset->indexcount, sizeof(uint32_t));

          memcpy((uint8_t*)lump->transfermemory + mesh->vertexbuffer.verticiesoffset, vertextable, mesh->vertexbuffer.vertexcount * mesh->vertexbuffer.vertexsize);
          memcpy((uint8_t*)lump->transfermemory + mesh->vertexbuffer.indicesoffset, indextable, mesh->vertexbuffer.indexcount * mesh->vertexbuffer.indexsize);

          update_vertexbuffer(lump->commandbuffer, lump->transferbuffer, slot->vertexbuffer);

          end(vulkan, lump->commandbuffer);

          submit_transfer(lump);

          slot->state = Mesh::State::Waiting;
        }
        else
          slot->state = Mesh::State::Empty;
      }
      else
        slot->state = Mesh::State::Empty;
    }
    else
      slot->state = Mesh::State::Empty;
  }

  Mesh::State waiting = Mesh::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, Mesh::State::Testing))
  {
    if (test(vulkan, slot->transferlump->fence))
    {
      release_lump(slot->transferlump);

      slot->transferlump = nullptr;

      slot->state = Mesh::State::Ready;
    }
    else
      slot->state = Mesh::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Mesh>(Mesh const *mesh)
{
  assert(mesh);

  defer_destroy(mesh);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Mesh>(Mesh const *mesh)
{
  assert(mesh);

  auto slot = const_cast<Mesh*>(mesh);

  if (slot->transferlump)
    release_lump(slot->transferlump);

  mesh->~Mesh();

  release_slot(slot, sizeof(Mesh));
}
