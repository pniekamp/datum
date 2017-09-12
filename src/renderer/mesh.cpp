//
// Datum - mesh
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "mesh.h"
#include "assetpack.h"
#include "debug.h"

using namespace std;
using namespace lml;
using leap::alignto;

namespace
{
  size_t mesh_datasize(int bonecount)
  {
    return bonecount * sizeof(Mesh::Bone);
  }

  size_t vertexbuffer_verticessize(int vertexcount, int indexcount, int bonecount)
  {
    return alignto(vertexcount * sizeof(Mesh::Vertex), size_t(4096));
  }

  size_t vertexbuffer_indicessize(int vertexcount, int indexcount, int bonecount)
  {
    return alignto(indexcount * sizeof(uint32_t), size_t(4096));
  }

  size_t vertexbuffer_rigsize(int vertexcount, int indexcount, int bonecount)
  {
    return (bonecount != 0) ? alignto(vertexcount * sizeof(Mesh::Rig), size_t(4096)) : 0;
  }

  size_t vertexbuffer_datasize(int vertexcount, int indexcount, int bonecount)
  {
    size_t verticessize = vertexbuffer_verticessize(vertexcount, indexcount, bonecount);
    size_t indicessize = vertexbuffer_indicessize(vertexcount, indexcount, bonecount);
    size_t rigsize = vertexbuffer_rigsize(vertexcount, indexcount, bonecount);

    return verticessize + indicessize + rigsize;
  }

  size_t vertexbuffer_verticesoffset(int vertexcount, int indexcount, int bonecount)
  {
    return 0;
  }

  size_t vertexbuffer_indicesoffset(int vertexcount, int indexcount, int bonecount)
  {
    return vertexbuffer_verticesoffset(vertexcount, indexcount, bonecount) + vertexbuffer_verticessize(vertexcount, indexcount, bonecount);
  }

  size_t vertexbuffer_rigoffset(int vertexcount, int indexcount, int bonecount)
  {
    return vertexbuffer_indicesoffset(vertexcount, indexcount, bonecount) + vertexbuffer_indicessize(vertexcount, indexcount, bonecount);
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

  auto slot = acquire_slot(sizeof(Mesh) + mesh_datasize(asset->bonecount));

  if (!slot)
    return nullptr;

  auto mesh = new(slot) Mesh;

  mesh->bound = Bound3(Vec3(asset->mincorner[0], asset->mincorner[1], asset->mincorner[2]), Vec3(asset->maxcorner[0], asset->maxcorner[1], asset->maxcorner[2]));
  mesh->bonecount = asset->bonecount;
  mesh->bones = nullptr;
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
  mesh->bonecount = 0;
  mesh->bones = nullptr;
  mesh->asset = nullptr;
  mesh->transferlump = nullptr;
  mesh->state = Mesh::State::Empty;

  auto setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
  auto setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  auto setupfence = create_fence(vulkan, 0);

  begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  mesh->vertexbuffer = create_vertexbuffer(vulkan, setupbuffer, vertexcount, sizeof(Mesh::Vertex), indexcount, sizeof(uint32_t));

  end(vulkan, setupbuffer);

  submit(setupbuffer, setupfence);

  wait_fence(vulkan, setupfence);

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

  update_vertexbuffer(lump->commandbuffer, lump->transferbuffer, slot->vertexbuffer.verticesoffset, slot->vertexbuffer.indicesoffset, slot->vertexbuffer);

  end(vulkan, lump->commandbuffer);

  submit(lump);

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
        auto vertextable = PackMeshPayload::vertextable(bits, asset->vertexcount, asset->indexcount);
        auto indextable = PackMeshPayload::indextable(bits, asset->vertexcount, asset->indexcount);

        if (auto lump = acquire_lump(vertexbuffer_datasize(asset->vertexcount, asset->indexcount, asset->bonecount)))
        {
          auto verticesoffset = vertexbuffer_verticesoffset(asset->vertexcount, asset->indexcount, asset->bonecount);
          auto indicesoffset = vertexbuffer_indicesoffset(asset->vertexcount, asset->indexcount, asset->bonecount);
          auto rigoffset = vertexbuffer_rigoffset(asset->vertexcount, asset->indexcount, asset->bonecount);

          wait_fence(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          slot->vertexbuffer = create_vertexbuffer(vulkan, lump->commandbuffer, asset->vertexcount, sizeof(Mesh::Vertex), asset->indexcount, sizeof(uint32_t));

          memcpy(lump->memory(verticesoffset), vertextable, slot->vertexbuffer.vertexcount * slot->vertexbuffer.vertexsize);
          memcpy(lump->memory(indicesoffset), indextable, slot->vertexbuffer.indexcount * slot->vertexbuffer.indexsize);

          update_vertexbuffer(lump->commandbuffer, lump->transferbuffer, verticesoffset, indicesoffset, slot->vertexbuffer);

          if (asset->bonecount != 0)
          {
            auto rigtable = PackMeshPayload::rigtable(bits, asset->vertexcount, asset->indexcount);

            slot->rigbuffer = create_vertexbuffer(vulkan, lump->commandbuffer, asset->vertexcount, sizeof(Mesh::Rig));

            memcpy(lump->memory(rigoffset), rigtable, slot->rigbuffer.vertexcount * slot->rigbuffer.vertexsize);

            update_vertexbuffer(lump->commandbuffer, lump->transferbuffer, rigoffset, slot->rigbuffer);
          }

          end(vulkan, lump->commandbuffer);

          submit(lump);

          slot->transferlump = lump;

          if (asset->bonecount != 0)
          {
            auto bonetable = PackMeshPayload::bonetable(bits, asset->vertexcount, asset->indexcount);

            auto bonedata = reinterpret_cast<Mesh::Bone*>(slot->data);

            memcpy(bonedata, bonetable, asset->bonecount*sizeof(Mesh::Bone));

            slot->bones = bonedata;
          }
        }
      }
    }

    slot->state = (slot->vertexbuffer) ? Mesh::State::Waiting : Mesh::State::Empty;
  }

  Mesh::State waiting = Mesh::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, Mesh::State::Testing))
  {
    bool ready = false;

    if (test_fence(vulkan, slot->transferlump->fence))
    {
      release_lump(slot->transferlump);

      slot->transferlump = nullptr;

      ready = true;
    }

    slot->state = (ready) ? Mesh::State::Ready : Mesh::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Mesh>(Mesh const *mesh)
{
  defer_destroy(mesh);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Mesh>(Mesh const *mesh)
{
  if (mesh)
  {
    if (mesh->transferlump)
      release_lump(mesh->transferlump);

    mesh->~Mesh();

    release_slot(const_cast<Mesh*>(mesh), sizeof(Mesh) + mesh_datasize(mesh->bonecount));
  }
}


///////////////////////// make_plane ////////////////////////////////////////
Mesh const *make_plane(ResourceManager &resources, int sizex, int sizey, float scale, float tilex, float tiley)
{
  auto mesh = resources.create<Mesh>(sizex*sizey, 6*(sizex-1)*(sizey-1));

  if (auto lump = resources.acquire_lump(mesh->vertexbuffer.size))
  {
    auto vertices = lump->memory<Mesh::Vertex>(mesh->vertexbuffer.verticesoffset);

    for(int y = 0; y < sizey; ++y)
    {
      for(int x = 0; x < sizex; ++x)
      {
        vertices->position = Vec3(2 * x/(sizex-1.0f) - 1, 2 * y/(sizey-1.0f) - 1, 0) * scale;
        vertices->normal = Vec3(0, 0, 1);
        vertices->tangent = Vec4(1, 0, 0, 1);
        vertices->texcoord = Vec2(tilex * x/(sizex-1.0f), tiley * y/(sizey-1.0f));

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
