//
// Datum - sprite list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "renderer.h"
#include "resource.h"
#include "commandlist.h"
#include <utility>


//|---------------------- SpriteList ----------------------------------------
//|--------------------------------------------------------------------------

class SpriteList
{
  public:

    operator CommandList const *() const { return m_commandlist; }

  public:

    struct BuildState
    {
      DatumPlatform::PlatformInterface *platform;

      RenderContext *context;
      ResourceManager *resources;

      VkDeviceSize offset;
      CommandList::Descriptor modelset;

      CommandList *commandlist = nullptr;
    };

    bool begin(BuildState &state, DatumPlatform::PlatformInterface &platform, RenderContext &context, ResourceManager *resources);

    void push_sprite(BuildState &state, lml::Vec2 xbasis, lml::Vec2 ybasis, lml::Vec2 position);
    void push_sprite(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect);
    void push_sprite(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, float rotation);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};
