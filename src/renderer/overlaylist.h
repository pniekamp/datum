//
// Datum - overlay list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "renderer.h"
#include "resource.h"
#include "commandlist.h"
#include "mesh.h"
#include "material.h"
#include <utility>

//|---------------------- OverlayList ---------------------------------------
//|--------------------------------------------------------------------------

class OverlayList
{
  public:

    operator bool() const { return m_commandlist; }

    CommandList const *commandlist() const { return m_commandlist; }

  public:

    struct BuildState
    {
      float depthfade = 0.0f;

      int clipx, clipy, clipwidth, clipheight;

      RenderContext *context;
      ResourceManager *resources;

      CommandList::Descriptor materialset;

      CommandList::Descriptor modelset;

      CommandList *commandlist = nullptr;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager &resources);

    void push_gizmo(BuildState &state, lml::Vec3 const &position, lml::Vec3 const &size, lml::Quaternion3f const &rotation, Mesh const *mesh, Material const *material, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_wireframe(BuildState &state, lml::Transform const &transform, Mesh const *mesh, lml::Color4 const &color);

    void push_stencilmask(BuildState &state, lml::Transform const &transform, Mesh const *mesh, uint32_t reference = 1);
    void push_stencilmask(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, uint32_t reference = 1);

    void push_stencilfill(BuildState &state, lml::Transform const &transform, Mesh const *mesh, lml::Color4 const &color, uint32_t reference = 1);
    void push_stencilfill(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Vec2 const &base, lml::Vec2 const &tiling, uint32_t reference = 1);

    void push_stencilpath(BuildState &state, lml::Transform const &transform, Mesh const *mesh, lml::Color4 const &color, float thickness = 1.0f, uint32_t reference = 1);
    void push_stencilpath(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Vec2 const &base, lml::Vec2 const &tiling, float thickness = 1.0f, uint32_t reference = 1);

    void push_line(BuildState &state, lml::Vec3 const &a, lml::Vec3 const &b, lml::Color4 const &color, float thickness = 1.0f);
    void push_lines(BuildState &state, lml::Vec3 const &position, lml::Vec3 const &size, lml::Quaternion3f const &rotation, Mesh const *mesh, lml::Color4 const &color, float thickness = 1.0f);

    void push_volume(BuildState &state, lml::Bound3 const &bound, Mesh const *mesh, lml::Color4 const &color, float thickness = 1.0f);

    void push_outline(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Color4 const &color);

    void push_scissor(BuildState &state, lml::Rect2 const &cliprect);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};
