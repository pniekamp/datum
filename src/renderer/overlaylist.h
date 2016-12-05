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
      DatumPlatform::PlatformInterface *platform;

      RenderContext *context;
      ResourceManager *resources;

      CommandList::Descriptor materialset;

      CommandList::Descriptor modelset;

      uintptr_t assetbarrier;

      CommandList *commandlist = nullptr;
    };

    bool begin(BuildState &state, DatumPlatform::PlatformInterface &platform, RenderContext &context, ResourceManager *resources);

    void push_gizmo(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material);
    void push_gizmo(BuildState &state, lml::Transform const &transform, lml::Vec3 const &scale, Mesh const *mesh, Material const *material);

    void push_wireframe(BuildState &state, lml::Transform const &transform, Mesh const *mesh, lml::Color4 const &color);

    void push_stencil(BuildState &state, lml::Transform const &transform, Mesh const *mesh, uint32_t reference = 1);
    void push_stencil(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, uint32_t reference = 1);

    void push_stencilfill(BuildState &state, lml::Transform const &transform, Mesh const *mesh, lml::Color4 const &color, uint32_t reference = 1);
    void push_stencilfill(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, uint32_t reference = 1);

    void push_outline(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Color4 const &color, uint32_t reference = 1);

    void push_volume(BuildState &state, lml::Bound3 const &bound, Mesh const *mesh, lml::Color4 const &color);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};
