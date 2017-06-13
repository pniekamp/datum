//
// Datum - geometry list
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

//|---------------------- GeometryList --------------------------------------
//|--------------------------------------------------------------------------

class GeometryList
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

    bool begin(BuildState &state, RenderContext &context, ResourceManager &resources);

    void push_mesh(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material);

    void push_mesh(BuildState &state, lml::Transform const &transform, Pose const &pose, Mesh const *mesh, Material const *material);

    void push_ocean(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Vec2 const &flow, lml::Vec3 const &bumpscale = { 1.0f, 1.0f, 1.0f }, lml::Plane const &foamplane = { { 0, 1, 0 }, 0 }, float foamwaveheight = 1.0f, float foamwavescale = 0.0f, float foamshoreheight = 0.1f, float foamshorescale = 0.1f);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};
