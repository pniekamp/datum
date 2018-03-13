//
// Datum - decal list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "renderer.h"
#include "resource.h"
#include "decal.h"
#include "commandlump.h"
#include <utility>

//|---------------------- DecalList -----------------------------------------
//|--------------------------------------------------------------------------

class DecalList
{
  public:

    Renderable::Decals::DecalList *decallist;

    explicit operator bool() const { return *m_commandlump; }

  public:

    struct BuildState
    {
      uint32_t decalmask = 0xFF;

      RenderContext *context;
      ResourceManager *resources;

      CommandLump *commandlump = nullptr;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager &resources);

    void push_decal(BuildState &state, lml::Transform const &transform, lml::Vec3 const &size, Decal const *decal, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_decal(BuildState &state, lml::Transform const &transform, lml::Vec3 const &size, Decal const *decal, float layer, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void finalise(BuildState &state);

  public:

    CommandLump const *release() { return m_commandlump.release(); }

  private:

    unique_resource<CommandLump> m_commandlump;
};
