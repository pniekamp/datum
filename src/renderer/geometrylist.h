//
// Datum - geometry list
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

//|---------------------- GeometryList --------------------------------------
//|--------------------------------------------------------------------------

class GeometryList
{
  public:

    VkCommandBuffer prepasscommands;
    VkCommandBuffer geometrycommands;

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

    void push_terrain(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Texture const *heightmap, Texture const *normalmap, lml::Rect2 const &region, float areascale, float morphbeg, float morphend, float morphgrid, Material const *material, Texture const *blendmap, int layers, lml::Vec2 const &uvscale = { 1.0f, 1.0f });

    void push_ocean(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Vec2 const &flow, lml::Vec3 const &bumpscale = { 1.0f, 1.0f, 1.0f }, lml::Plane const &foamplane = { { 0, 1, 0 }, 0 }, float foamwaveheight = 1.0f, float foamwavescale = 0.0f, float foamshoreheight = 0.1f, float foamshorescale = 0.1f);

    void finalise(BuildState &state);

  public:

    void bind_material(BuildState &state, Material const *material);

    CommandLump const *release() { return m_commandlump.release(); }

  private:

    unique_resource<CommandLump> m_commandlump;
};
