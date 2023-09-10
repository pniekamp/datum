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
    return alignto(vertexcount * sizeof(Mesh::Vertex), size_t(256));
  }

  size_t vertexbuffer_indicessize(int vertexcount, int indexcount, int bonecount)
  {
    return alignto(indexcount * sizeof(uint32_t), size_t(256));
  }

  size_t vertexbuffer_rigsize(int vertexcount, int indexcount, int bonecount)
  {
    return (bonecount != 0) ? alignto(vertexcount * sizeof(Mesh::Rig), size_t(256)) : 0;
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

VkDeviceSize Mesh::size() const { return vertexbuffer_datasize(vertexbuffer.vertexcount, vertexbuffer.indexcount, bonecount) + mesh_datasize(bonecount); }
VkDeviceSize Mesh::verticesoffset() const { return vertexbuffer_verticesoffset(vertexbuffer.vertexcount, vertexbuffer.indexcount, bonecount); }
VkDeviceSize Mesh::indicesoffset() const { return vertexbuffer_indicesoffset(vertexbuffer.vertexcount, vertexbuffer.indexcount, bonecount); }
VkDeviceSize Mesh::rigoffset() const { return vertexbuffer_rigoffset(vertexbuffer.vertexcount, vertexbuffer.indexcount, bonecount); }
VkDeviceSize Mesh::bonesoffset() const { return vertexbuffer_datasize(vertexbuffer.vertexcount, vertexbuffer.indexcount, bonecount); }

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
Mesh const *ResourceManager::create<Mesh>(size_t vertexcount, size_t indexcount)
{
  return create<Mesh>((int)vertexcount, (int)indexcount);
}

template<>
Mesh const *ResourceManager::create<Mesh>(uint32_t vertexcount, uint32_t indexcount)
{
  return create<Mesh>((int)vertexcount, (int)indexcount);
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Mesh const *ResourceManager::create<Mesh>(int vertexcount, int indexcount, int bonecount)
{
  auto slot = acquire_slot(sizeof(Mesh) + mesh_datasize(bonecount));

  if (!slot)
    return nullptr;

  auto mesh = new(slot) Mesh;

  mesh->bound = {};
  mesh->bonecount = bonecount;
  mesh->bones = nullptr;
  mesh->asset = nullptr;
  mesh->transferlump = nullptr;
  mesh->state = Mesh::State::Empty;

  auto setuppool = create_commandpool(vulkan, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
  auto setupbuffer = allocate_commandbuffer(vulkan, setuppool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  auto setupfence = create_fence(vulkan, 0);

  begin(vulkan, setupbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  mesh->vertexbuffer = create_vertexbuffer(vulkan, setupbuffer, vertexcount, sizeof(Mesh::Vertex), indexcount, sizeof(uint32_t));
  mesh->rigbuffer = create_vertexbuffer(vulkan, setupbuffer, vertexcount, sizeof(Mesh::Rig));
  mesh->bones = reinterpret_cast<Mesh::Bone*>(mesh->data);

  end(vulkan, setupbuffer);

  submit(setupbuffer, setupfence);

  wait_fence(vulkan, setupfence);

  mesh->state = Mesh::State::Ready;

  return mesh;
}

template<>
Mesh const *ResourceManager::create<Mesh>(size_t vertexcount, size_t indexcount, size_t bonecount)
{
  return create<Mesh>((int)vertexcount, (int)indexcount, (int)bonecount);
}

template<>
Mesh const *ResourceManager::create<Mesh>(uint32_t vertexcount, uint32_t indexcount, uint32_t bonecount)
{
  return create<Mesh>((int)vertexcount, (int)indexcount, (int)bonecount);
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Mesh>(Mesh const *mesh, ResourceManager::TransferLump const *lump, Bound3 bound)
{
  assert(lump);
  assert(mesh);
  assert(mesh->state == Mesh::State::Ready);

  auto slot = const_cast<Mesh*>(mesh);

  wait_fence(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  auto verticesoffset = slot->verticesoffset();
  auto indicesoffset = slot->indicesoffset();
  auto rigoffset = slot->rigoffset();

  auto verticessize = slot->vertexbuffer.vertexcount * slot->vertexbuffer.vertexsize;
  auto indicessize = slot->vertexbuffer.indexcount * slot->vertexbuffer.indexsize;
  auto rigsize = slot->rigbuffer.vertexcount * slot->rigbuffer.vertexsize;

  blit(lump->commandbuffer, lump->transferbuffer, verticesoffset, slot->vertexbuffer.vertices, 0, verticessize);
  blit(lump->commandbuffer, lump->transferbuffer, indicesoffset, slot->vertexbuffer.indices, 0, indicessize);

  if (slot->bonecount != 0)
  {
    blit(lump->commandbuffer, lump->transferbuffer, rigoffset, slot->rigbuffer.vertices, 0, rigsize);

    memcpy(reinterpret_cast<Mesh::Bone*>(slot->data), lump->memory(slot->bonesoffset()), slot->bonecount*sizeof(Mesh::Bone));
  }

  end(vulkan, lump->commandbuffer);

  submit(lump);

  while (!test_fence(vulkan, lump->fence))
    ;

  slot->bound = bound;
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Mesh>(Mesh const *mesh, ResourceManager::TransferLump const *lump, uint32_t srcoffset, uint32_t dstoffset, uint32_t size, Bound3 bound)
{
  assert(lump);
  assert(mesh);
  assert(mesh->state == Mesh::State::Ready);

  auto slot = const_cast<Mesh*>(mesh);

  wait_fence(vulkan, lump->fence);

  begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  // NOTE: only supports bounded subsets of specific transfer regions

  auto verticessize = slot->vertexbuffer.vertexcount * slot->vertexbuffer.vertexsize;
  auto verticesoffset = slot->verticesoffset();

  if (verticesoffset <= dstoffset && dstoffset < verticesoffset + verticessize)
  {
    assert(dstoffset + size <= verticesoffset + verticessize);

    blit(lump->commandbuffer, lump->transferbuffer, srcoffset, slot->vertexbuffer.vertices, dstoffset - verticesoffset, size);
  }

  auto indicessize = slot->vertexbuffer.indexcount * slot->vertexbuffer.indexsize;
  auto indicesoffset = slot->indicesoffset();

  if (indicesoffset <= dstoffset && dstoffset < indicesoffset + indicessize)
  {
    assert(dstoffset + size <= indicesoffset + indicessize);

    blit(lump->commandbuffer, lump->transferbuffer, srcoffset, slot->vertexbuffer.indices, dstoffset - indicesoffset, size);
  }

  auto rigsize = slot->rigbuffer.vertexcount * slot->rigbuffer.vertexsize;
  auto rigoffset = slot->rigoffset();

  if (slot->bonecount != 0 && rigoffset <= dstoffset && dstoffset < rigoffset + rigsize)
  {
    assert(dstoffset + size <= rigoffset + rigsize);

    blit(lump->commandbuffer, lump->transferbuffer, srcoffset, slot->rigbuffer.vertices, dstoffset - rigoffset, size);
  }

  auto bonessize = slot->bonecount * sizeof(Mesh::Bone);
  auto bonesoffset = slot->bonesoffset();

  if (slot->bonecount != 0 && bonesoffset <= dstoffset && dstoffset < bonesoffset + bonessize)
  {
    assert(dstoffset + size <= bonesoffset + bonessize);

    memcpy(reinterpret_cast<Mesh::Bone*>(slot->data + dstoffset - bonesoffset), lump->memory(srcoffset), size);
  }

  end(vulkan, lump->commandbuffer);

  submit(lump);

  while (!test_fence(vulkan, lump->fence))
    ;

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
        if (auto lump = acquire_lump(vertexbuffer_datasize(asset->vertexcount, asset->indexcount, asset->bonecount)))
        {
          wait_fence(vulkan, lump->fence);

          begin(vulkan, lump->commandbuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

          if (create_vertexbuffer(vulkan, lump->commandbuffer, asset->vertexcount, sizeof(Mesh::Vertex), asset->indexcount, sizeof(uint32_t), 0, &slot->vertexbuffer))
          {
            auto verticessize = slot->vertexbuffer.vertexcount * slot->vertexbuffer.vertexsize;
            auto verticesoffset = vertexbuffer_verticesoffset(asset->vertexcount, asset->indexcount, asset->bonecount);
            auto vertextable = PackMeshPayload::vertextable(bits, asset->vertexcount, asset->indexcount);

            memcpy(lump->memory(verticesoffset), vertextable, verticessize);
            blit(lump->commandbuffer, lump->transferbuffer, verticesoffset, slot->vertexbuffer.vertices, 0, verticessize);

            auto indicessize = slot->vertexbuffer.indexcount * slot->vertexbuffer.indexsize;
            auto indicesoffset = vertexbuffer_indicesoffset(asset->vertexcount, asset->indexcount, asset->bonecount);
            auto indextable = PackMeshPayload::indextable(bits, asset->vertexcount, asset->indexcount);

            memcpy(lump->memory(indicesoffset), indextable, indicessize);
            blit(lump->commandbuffer, lump->transferbuffer, indicesoffset, slot->vertexbuffer.indices, 0, indicessize);

            if (asset->bonecount != 0)
            {
              if (create_vertexbuffer(vulkan, lump->commandbuffer, asset->vertexcount, sizeof(Mesh::Rig), 0, &slot->rigbuffer))
              {
                auto rigsize = slot->rigbuffer.vertexcount * slot->rigbuffer.vertexsize;
                auto rigoffset = vertexbuffer_rigoffset(asset->vertexcount, asset->indexcount, asset->bonecount);
                auto rigtable = PackMeshPayload::rigtable(bits, asset->vertexcount, asset->indexcount);

                memcpy(lump->memory(rigoffset), rigtable, rigsize);
                blit(lump->commandbuffer, lump->transferbuffer, rigoffset, slot->rigbuffer.vertices, 0, rigsize);
              }
            }
          }

          end(vulkan, lump->commandbuffer);

          submit(lump);

          if (asset->bonecount != 0)
          {
            auto bonetable = PackMeshPayload::bonetable(bits, asset->vertexcount, asset->indexcount);

            auto bonedata = reinterpret_cast<Mesh::Bone*>(slot->data);

            memcpy(bonedata, bonetable, asset->bonecount*sizeof(Mesh::Bone));

            slot->bones = bonedata;
          }

          if (!slot->vertexbuffer || !(slot->bonecount == 0 || slot->rigbuffer))
          {
            release_lump(lump);
          }
          else
          {
            slot->transferlump = lump;
          }
        }
      }
    }

    slot->state = (slot->vertexbuffer && (slot->bonecount == 0 || slot->rigbuffer)) ? Mesh::State::Waiting : Mesh::State::Empty;
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

    auto datasize = mesh_datasize(mesh->bonecount);

    mesh->~Mesh();

    release_slot(const_cast<Mesh*>(mesh), sizeof(Mesh) + datasize);
  }
}


///////////////////////// make_plane ////////////////////////////////////////
Mesh const *make_plane(ResourceManager &resources, int sizex, int sizey, float scale, float tilex, float tiley)
{
  auto mesh = resources.create<Mesh>(sizex*sizey, 6*(sizex-1)*(sizey-1));

  if (auto lump = resources.acquire_lump(mesh->size()))
  {
    auto vertices = lump->memory<Mesh::Vertex>(mesh->verticesoffset());

    for(int y = 0; y < sizey; ++y)
    {
      for(int x = 0; x < sizex; ++x)
      {
        vertices->position = Vec3(2 * x/(sizex-1.0f) - 1, 2 * y/(sizey-1.0f) - 1, 0) * scale;
        vertices->normal = Vec3(0, 0, 1);
        vertices->tangent = Vec4(1, 0, 0, -1);
        vertices->texcoord = Vec2(tilex * x/(sizex-1.0f), tiley * y/(sizey-1.0f));

        ++vertices;
      }
    }

    auto indices = lump->memory<uint32_t>(mesh->indicesoffset());

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

    resources.update(mesh, lump, Bound3(Vec3(-scale, -scale, 0.0f), Vec3(scale, scale, 0.0f)));

    resources.release_lump(lump);
  }

  return mesh;
}
