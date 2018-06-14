//
// Datum - mesh
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "resource.h"

//|---------------------- Mesh ----------------------------------------------
//|--------------------------------------------------------------------------

class Mesh
{
  public:

    struct Vertex
    {
      lml::Vec3 position;
      lml::Vec2 texcoord;
      lml::Vec3 normal;
      lml::Vec4 tangent;
    };

    struct Bone
    {
      char name[32];
      lml::Transform transform;
    };

    struct Rig
    {
      uint32_t bone[4];
      float weight[4];
    };

    VkDeviceSize size() const;
    VkDeviceSize verticesoffset() const;
    VkDeviceSize indicesoffset() const;
    VkDeviceSize rigoffset() const;
    VkDeviceSize bonesoffset() const;

  public:
    friend Mesh const *ResourceManager::create<Mesh>(Asset const *asset);
    friend Mesh const *ResourceManager::create<Mesh>(int vertexcount, int indexcount);
    friend Mesh const *ResourceManager::create<Mesh>(int vertexcount, int indexcount, int bonecount);

    friend void ResourceManager::update<Mesh>(Mesh const *mesh, ResourceManager::TransferLump const *lump, lml::Bound3 bound);
    friend void ResourceManager::update<Mesh>(Mesh const *mesh, ResourceManager::TransferLump const *lump, uint32_t srcoffset, uint32_t dstoffset, uint32_t size, lml::Bound3 bound);

    bool ready() const { return (state == State::Ready); }

    lml::Bound3 bound;

    Vulkan::VertexBuffer vertexbuffer;

    Vulkan::VertexBuffer rigbuffer;

    int bonecount;
    Bone const *bones;

  public:

    enum class State
    {
      Empty,
      Loading,
      Waiting,
      Testing,
      Ready,
    };

    Asset const *asset;
    ResourceManager::TransferLump const *transferlump;

    std::atomic<State> state;

    alignas(16) uint8_t data[1];

  protected:
    Mesh() = default;
};

Mesh const *make_plane(ResourceManager &resources, int sizex, int sizey, float scale = 1.0f, float tilex = 1.0f, float tiley = 1.0f);
