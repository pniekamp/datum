//
// Datum - overlay list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "renderer.h"
#include "resource.h"
#include "mesh.h"
#include "material.h"
#include "commandlump.h"
#include <utility>

//|---------------------- OverlayList ---------------------------------------
//|--------------------------------------------------------------------------

class OverlayList
{
  public:

    VkCommandBuffer overlaycommands;

    explicit operator bool() const { return *m_commandlump; }

  public:

    struct BuildState
    {
      float depthfade = 0.0f;

      RenderContext *context;
      ResourceManager *resources;

      CommandLump::Descriptor materialset;

      CommandLump::Descriptor modelset;

      int clipx, clipy, clipwidth, clipheight;

      lml::Rect2 cliprect() const { return { lml::Vec2(clipx, clipy), lml::Vec2(clipx + clipwidth, clipy + clipheight) }; }

      CommandLump *commandlump = nullptr;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager &resources);

    void push_gizmo(BuildState &state, lml::Vec3 const &position, lml::Vec3 const &size, lml::Quaternion3 const &rotation, Mesh const *mesh, Material const *material, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_wireframe(BuildState &state, lml::Transform const &transform, Mesh const *mesh, lml::Color4 const &color);

    void push_stencilmask(BuildState &state, lml::Transform const &transform, Mesh const *mesh, uint32_t reference = 1);
    void push_stencilmask(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, uint32_t reference = 1);

    void push_stencilfill(BuildState &state, lml::Transform const &transform, Mesh const *mesh, lml::Color4 const &color, uint32_t reference = 1);
    void push_stencilfill(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Vec2 const &base, lml::Vec2 const &tiling, uint32_t reference = 1);

    void push_stencilpath(BuildState &state, lml::Transform const &transform, Mesh const *mesh, lml::Color4 const &color, float thickness = 1.0f, uint32_t reference = 1);
    void push_stencilpath(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Vec2 const &base, lml::Vec2 const &tiling, float thickness = 1.0f, uint32_t reference = 1);

    void push_line(BuildState &state, lml::Vec3 const &a, lml::Vec3 const &b, lml::Color4 const &color, float thickness = 1.0f);
    void push_lines(BuildState &state, lml::Vec3 const &position, lml::Vec3 const &size, lml::Quaternion3 const &rotation, Mesh const *mesh, lml::Color4 const &color, float thickness = 1.0f);

    void push_volume(BuildState &state, lml::Bound3 const &bound, Mesh const *mesh, lml::Color4 const &color, float thickness = 1.0f);

    void push_outline(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Color4 const &color);

    void push_scissor(BuildState &state, lml::Rect2 const &cliprect);

    void finalise(BuildState &state);

  public:

    CommandLump const *release() { return m_commandlump.release(); }

  private:

    unique_resource<CommandLump> m_commandlump;
};
