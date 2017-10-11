//
// Datum - light list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "renderer.h"
#include "resource.h"
#include "spotmap.h"
#include "commandlump.h"
#include <utility>

//|---------------------- LightList -----------------------------------------
//|--------------------------------------------------------------------------

class LightList
{
  public:

    Renderable::Lights::LightList *lightlist;

    operator bool() const { return m_commandlump; }

  public:

    struct BuildState
    {
      RenderContext *context;
      ResourceManager *resources;

      CommandLump *commandlump = nullptr;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager &resources);

    void push_pointlight(BuildState &state, lml::Vec3 const &position, float range, lml::Color3 const &intensity, lml::Attenuation const &attenuation);

    void push_spotlight(BuildState &state, lml::Vec3 const &position, lml::Vec3 const &direction, float cutoff, float range, lml::Color3 const &intensity, lml::Attenuation const &attenuation, lml::Transform const &spotview, SpotMap const *spotmap);

    void push_probe(BuildState &state, lml::Vec3 const &position, float radius, Irradiance const &irradiance);

    void push_environment(BuildState &state, lml::Transform const &transform, lml::Vec3 const &size, EnvMap const *envmap);

    void finalise(BuildState &state);

  public:

    CommandLump const *release() { return m_commandlump.release(); }

  private:

    unique_resource<CommandLump> m_commandlump;
};
