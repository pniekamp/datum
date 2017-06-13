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

    operator bool() const { return m_commandlist; }

    Renderable::Lights::LightList const *lightlist() const { return m_lights; }

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

    void finalise(BuildState &state);

  private:

    Renderable::Lights::LightList *m_lights;

    unique_resource<CommandList> m_commandlist;
};
