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

    friend void ResourceManager::update<Mesh>(Mesh const *Mesh);
    friend void ResourceManager::update<Mesh>(Mesh const *Mesh, Vertex const *vertices, uint32_t const *indices);
    friend void ResourceManager::update<Mesh>(Mesh const *Mesh, Vertex const *vertices);
    friend void ResourceManager::update<Mesh>(Mesh const *mesh, lml::Bound3 bound);

    bool ready() const { return (state == State::Ready); }

    lml::Bound3 bound;

    Vulkan::VertexBuffer vertexbuffer;

    Vertex *vertices;
    uint32_t *indices;

  public:

    enum class State
    {
      Empty,
      Loading,
      Waiting,
      Testing,
      Ready,
    };

    std::atomic<State> state;

    ResourceManager::TransferLump const *transferlump;

  private:
    Mesh() = default;
};
