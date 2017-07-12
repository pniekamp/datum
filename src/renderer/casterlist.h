//
// Datum - caster list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "renderer.h"
#include "resource.h"
#include "mesh.h"
#include "material.h"
#include "animation.h"
#include "commandlump.h"
#include <utility>

//|---------------------- CasterList ----------------------------------------
//|--------------------------------------------------------------------------

class CasterList
{
  public:

    VkCommandBuffer castercommands;

    operator bool() const { return m_commandlump; }

  public:

    struct BuildState
    {
      RenderContext *context;
      ResourceManager *resources;

      VkPipeline pipeline;

      CommandLump::Descriptor materialset;

      CommandLump::Descriptor modelset;

      CommandLump *commandlump = nullptr;

      Mesh const *mesh;
      Material const *material;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager &resources);

    void push_mesh(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material);

    void push_mesh(BuildState &state, lml::Transform const &transform, Pose const &pose, Mesh const *mesh, Material const *material);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandLump> m_commandlump;
};
