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
#include "animation.h"
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
      RenderContext *context;
      ResourceManager *resources;

      VkPipeline pipeline;

      CommandList::Descriptor materialset;

      CommandList::Descriptor modelset;

      CommandList *commandlist = nullptr;

      Mesh const *mesh;
      Material const *material;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager *resources);

    void push_mesh(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material);

    void push_mesh(BuildState &state, lml::Transform const &transform, Pose const &pose, Mesh const *mesh, Material const *material);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};
