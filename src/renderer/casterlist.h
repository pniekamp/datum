//
// Datum - caster list
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

//|---------------------- CasterList ----------------------------------------
//|--------------------------------------------------------------------------

class CasterList
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

      uintptr_t assetbarrier;

      CommandList::Descriptor materialset;

      CommandList::Descriptor modelset;

      CommandList *commandlist = nullptr;

      Mesh const *mesh;
      Material const *material;
    };

    bool begin(BuildState &state, DatumPlatform::PlatformInterface &platform, RenderContext &context, ResourceManager *resources);

    void push_material(BuildState &state, Material const *material);

    void push_mesh(BuildState &state, lml::Transform const &transform, Mesh const *mesh);

    void push_mesh(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};
