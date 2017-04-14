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
#include <utility>

//|---------------------- LightList -----------------------------------------
//|--------------------------------------------------------------------------

class LightList
{
  public:

    operator bool() const { return m_commandlist; }

    CommandList const *commandlist() const { return m_commandlist; }

  public:

    struct Lights
    {
      size_t pointlightcount;

      struct PointLight
      {
        lml::Vec3 position;
        lml::Color3 intensity;
        lml::Vec4 attenuation;

      } pointlights[256];
    };

    struct BuildState
    {
      RenderContext *context;
      ResourceManager *resources;

      Lights *lights;

      CommandList *commandlist = nullptr;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager *resources);

    void push_pointlight(BuildState &state, lml::Vec3 const &position, float range, lml::Color3 const &intensity, lml::Attenuation const &attenuation);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};
