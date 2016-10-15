//
// Datum - mesh
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "resource.h"

//|---------------------- Vertex --------------------------------------------
//|--------------------------------------------------------------------------

struct Vertex
{
  lml::Vec3 position;
  lml::Vec2 texcoord;
  lml::Vec3 normal;
  lml::Vec4 tangent;
};


//|---------------------- Mesh ----------------------------------------------
//|--------------------------------------------------------------------------

class Mesh
{
  public:
    friend Mesh const *ResourceManager::create<Mesh>(Asset const *asset);
    friend Mesh const *ResourceManager::create<Mesh>(int vertexcount, int indexcount);

    friend void ResourceManager::update<Mesh>(Mesh const *mesh, ResourceManager::TransferLump const *lump);
    friend void ResourceManager::update<Mesh>(Mesh const *mesh, lml::Bound3 bound);

    bool ready() const { return (state == State::Ready); }

    lml::Bound3 bound;

    Vulkan::VertexBuffer vertexbuffer;

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

  private:
    Mesh() = default;
};
