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
#include "sprite.h"
#include "font.h"
#include <utility>

//|---------------------- SpriteList ----------------------------------------
//|--------------------------------------------------------------------------

class SpriteList
{
  public:

    operator bool() const { return m_commandlist; }

    CommandList const *commandlist() const { return m_commandlist; }

  public:

    struct BuildState
    {
      RenderContext *context;
      ResourceManager *resources;

      CommandList::Descriptor materialset;

      CommandList::Descriptor modelset;

      CommandList *commandlist = nullptr;

      VkSampler texture;
      lml::Color4 color;
      lml::Vec4 texcoords;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager *resources);

    void push_material(BuildState &state, Vulkan::Texture const &texture, lml::Vec4 const &texcoords, lml::Color4 const &tint);

    void push_model(BuildState &state, lml::Vec2 xbasis, lml::Vec2 ybasis, lml::Vec2 position, float layer);

    void push_line(BuildState &state, lml::Vec2 const &a, lml::Vec2 const &b, lml::Color4 const &color, float thickness = 1.0f);

    void push_rect(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, lml::Color4 const &color);
    void push_rect(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, float rotation, lml::Color4 const &color);

    void push_rect_outline(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, lml::Color4 const &color, float thickness = 1.0f);
    void push_rect_outline(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, float rotation, lml::Color4 const &color, float thickness = 1.0f);

    void push_sprite(BuildState &state, lml::Vec2 const &position, float size, Sprite const *sprite, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, float size, Sprite const *sprite, float layer, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, float size, float rotation, Sprite const *sprite, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, float size, float rotation, Sprite const *sprite, float layer, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_sprite(BuildState &state, lml::Vec2 const &xbasis, lml::Vec2 const &ybasis, lml::Vec2 const &position, float size, Sprite const *sprite, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &xbasis, lml::Vec2 const &ybasis, lml::Vec2 const &position, float size, Sprite const *sprite, float layer, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_text(BuildState &state, lml::Vec2 const &position, float size, Font const *font, const char *str, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_text(BuildState &state, lml::Vec2 const &position, float size, float rotation, Font const *font, const char *str, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_text(BuildState &state, lml::Vec2 const &xbasis, lml::Vec2 const &ybasis, lml::Vec2 const &position, float size, Font const *font, const char *str, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_texture(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, Vulkan::Texture const &texture, float layer = 0, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_scissor(BuildState &state, lml::Rect2 const &cliprect);

    void finalise(BuildState &state);

  public:

    void viewport(lml::Rect2 const &viewport) const;

  private:

    void *m_sceneset;

    unique_resource<CommandList> m_commandlist;
};
