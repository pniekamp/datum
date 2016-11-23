//
// Datum - forward list
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

//|---------------------- ForwardList ---------------------------------------
//|--------------------------------------------------------------------------

class ForwardList
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

    void push_transparent(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};