//
// Datum - light list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "renderer.h"
#include "resource.h"
#include "commandlist.h"
#include "mesh.h"
#include <utility>

//|---------------------- LightList -----------------------------------------
//|--------------------------------------------------------------------------

class LightList
{
  public:

    Renderable::Lights::LightList *lightlist;

    operator bool() const { return m_commandlist; }

  public:

    struct BuildState
    {
      RenderContext *context;
      ResourceManager *resources;

      CommandList *commandlist = nullptr;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager &resources);

    void push_pointlight(BuildState &state, lml::Vec3 const &position, float range, lml::Color3 const &intensity, lml::Attenuation const &attenuation);

    void push_spotlight(BuildState &state, lml::Vec3 const &position, lml::Vec3 const &direction, float cutoff, float range, lml::Color3 const &intensity, lml::Attenuation const &attenuation);

    void push_environment(BuildState &state, lml::Transform const &transform, lml::Vec3 const &dimension, EnvMap const *envmap);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};
