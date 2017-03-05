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
  mesh->asset = asset;
  mesh->transferlump = nullptr;
  mesh->state = Mesh::State::Empty;

  return mesh;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Mesh const *ResourceManager::create<Mesh>(int vertexcount, int indexcount)
{
  auto slot = acquire_slot(sizeof(Mesh));

  if (!slot)
    return nullptr;

  auto mesh = new(slot) Mesh;

  mesh->bound = {};
  mesh->asset = nullptr;
  mesh->transferlump = nullptr;
  mesh->state = Mesh::State::Empty;

  auto lump = acquire_lump(0);

  if (!lump)
  {
    mesh->~Mesh();

    release_slot(mesh, sizeof(Mesh));

    return nullptr;
  }

  wait_fence(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  mesh->vertexbuffer = create_vertexbuffer(vulkan, lump->commandbuffer, vertexcount, sizeof(Vertex), indexcount, sizeof(uint32_t));

  end(vulkan, lump->commandbuffer);

  submit_transfer(lump);

  release_lump(lump);

  mesh->state = Mesh::State::Ready;

  return mesh;
}

template<>
Mesh const *ResourceManager::create<Mesh>(uint32_t vertexcount, uint32_t indexcount)
{
  return create<Mesh>((int)vertexcount, (int)indexcount);
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Mesh>(Mesh const *mesh, ResourceManager::TransferLump const *lump)
{
  assert(lump);
  assert(mesh);
  assert(mesh->state == Mesh::State::Ready);

  auto slot = const_cast<Mesh*>(mesh);

  wait_fence(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  update_vertexbuffer(lump->commandbuffer, lump->transferbuffer, slot->vertexbuffer);

  end(vulkan, lump->commandbuffer);

  submit_transfer(lump);

  while (!test_fence(vulkan, lump->fence))
    ;
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
    if (auto asset = slot->asset)
    {
      assert(assets()->barriercount != 0);

      if (auto bits = m_assets->request(platform, asset))
      {
        auto payload = reinterpret_cast<PackMeshPayload const *>(bits);

        auto vertextable = PackMeshPayload::vertextable(payload, asset->vertexcount, asset->indexcount);
        auto indextable = PackMeshPayload::indextable(payload, asset->vertexcount, asset->indexcount);

        if (auto lump = acquire_lump(mesh_datasize(asset->vertexcount, asset->indexcount)))
        {
          wait_fence(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          slot->vertexbuffer = create_vertexbuffer(vulkan, lump->commandbuffer, asset->vertexcount, sizeof(Vertex), asset->indexcount, sizeof(uint32_t));

          memcpy((uint8_t*)lump->transfermemory + slot->vertexbuffer.verticesoffset, vertextable, slot->vertexbuffer.vertexcount * slot->vertexbuffer.vertexsize);
          memcpy((uint8_t*)lump->transfermemory + slot->vertexbuffer.indicesoffset, indextable, slot->vertexbuffer.indexcount * slot->vertexbuffer.indexsize);

          update_vertexbuffer(lump->commandbuffer, lump->transferbuffer, slot->vertexbuffer);

          end(vulkan, lump->commandbuffer);

          submit_transfer(lump);

          slot->transferlump = lump;

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
    if (test_fence(vulkan, slot->transferlump->fence))
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

  if (mesh->transferlump)
    release_lump(mesh->transferlump);

  mesh->~Mesh();

  release_slot(const_cast<Mesh*>(mesh), sizeof(Mesh));
}


///////////////////////// make_plane ////////////////////////////////////////
Mesh const *make_plane(ResourceManager &resources, int sizex, int sizey, float tilex, float tiley)
{
  auto mesh = resources.create<Mesh>(sizex*sizey, 6*(sizex-1)*(sizey-1));

  if (auto lump = resources.acquire_lump(mesh->vertexbuffer.size))
  {
    auto vertices = lump->memory<Vertex>(mesh->vertexbuffer.verticesoffset);

    for(int y = 0; y < sizey; ++y)
    {
      for(int x = 0; x < sizex; ++x)
      {
        vertices->position = Vec3(x, y, 0);
        vertices->normal = Vec3(0, 0, 1);
        vertices->tangent = Vec4(1, 0, 0, 1);
        vertices->texcoord = Vec2((x * tilex)/(sizex-1), (y * tiley)/(sizey-1));

        ++vertices;
      }
    }

    auto indices = lump->memory<uint32_t>(mesh->vertexbuffer.indicesoffset);

    for(int y = 0; y < sizey-1; ++y)
    {
      for(int x = 0; x < sizex-1; ++x)
      {
        *indices++ = (y+1)*sizex + (x+0);
        *indices++ = (y+0)*sizex + (x+0);
        *indices++ = (y+1)*sizex + (x+1);
        *indices++ = (y+1)*sizex + (x+1);
        *indices++ = (y+0)*sizex + (x+0);
        *indices++ = (y+0)*sizex + (x+1);
      }
    }

    resources.update(mesh, lump);

    resources.release_lump(lump);
  }

  return mesh;
}
