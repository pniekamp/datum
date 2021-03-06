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

    explicit operator bool() const { return *m_commandlump; }

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

    void push_foilage(BuildState &state, lml::Transform const *transforms, size_t count, Mesh const *mesh, Material const *material, lml::Vec4 const &wind = { 0.0f, 0.0f, 0.0f, 0.0f }, lml::Vec3 const &bendscale = { 0.0f, 0.025f, 0.0f }, lml::Vec3 const &detailbendscale = { 0.0f, 0.025f, 0.0f });

    void finalise(BuildState &state);

  public:

    CommandLump const *release() { return m_commandlump.release(); }

  private:

    unique_resource<CommandLump> m_commandlump;
};
