//
// Datum - sprite list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "renderer.h"
#include "resource.h"
#include "sprite.h"
#include "font.h"
#include "commandlump.h"
#include <utility>

//|---------------------- SpriteList ----------------------------------------
//|--------------------------------------------------------------------------

class SpriteList
{
  public:

    VkCommandBuffer spritecommands;

    explicit operator bool() const { return *m_commandlump; }

  public:

    struct BuildState
    {
      RenderContext *context;
      ResourceManager *resources;

      CommandLump::Descriptor materialset;

      CommandLump::Descriptor modelset;

      int clipx, clipy, clipwidth, clipheight;

      lml::Rect2 cliprect() const { return { lml::Vec2(clipx, clipy), lml::Vec2(clipx + clipwidth, clipy + clipheight) }; }

      CommandLump *commandlump = nullptr;

      VkImageView texture;
      lml::Color4 color;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager &resources);

    void push_material(BuildState &state, Vulkan::Texture const &texture, lml::Color4 const &tint);

    void push_model(BuildState &state, lml::Vec2 const &position, lml::Vec2 const &xbasis, lml::Vec2 const &ybasis, lml::Vec4 const &extent, float layer0, float layer1);

    void push_line(BuildState &state, lml::Vec2 const &a, lml::Vec2 const &b, lml::Color4 const &color, float thickness = 1.0f);

    void push_rect(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, lml::Color4 const &color);
    void push_rect(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, float rotation, lml::Color4 const &color);

    void push_rect_outline(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, lml::Color4 const &color, float thickness = 1.0f);
    void push_rect_outline(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, float rotation, lml::Color4 const &color, float thickness = 1.0f);

    void push_sprite(BuildState &state, lml::Vec2 const &position, float size, Sprite const *sprite, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, float size, Sprite const *sprite, float layer, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, float size, float rotation, Sprite const *sprite, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, float size, float rotation, Sprite const *sprite, float layer, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_sprite(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, Sprite const *sprite, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, Sprite const *sprite, float layer, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, float rotation, Sprite const *sprite, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, float rotation, Sprite const *sprite, float layer, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_sprite(BuildState &state, lml::Vec2 const &position, lml::Vec2 const &xbasis, lml::Vec2 const &ybasis, Sprite const *sprite, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, lml::Vec2 const &xbasis, lml::Vec2 const &ybasis, Sprite const *sprite, float layer, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_sprite(BuildState &state, lml::Vec2 const &position, lml::Vec2 const &xbasis, lml::Vec2 const &ybasis, Sprite const *sprite, lml::Rect2 const &region, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_sprite(BuildState &state, lml::Vec2 const &position, lml::Vec2 const &xbasis, lml::Vec2 const &ybasis, Sprite const *sprite, lml::Rect2 const &region, float layer, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_text(BuildState &state, lml::Vec2 const &position, float size, Font const *font, const char *str, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });
    void push_text(BuildState &state, lml::Vec2 const &position, float size, float rotation, Font const *font, const char *str, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_text(BuildState &state, lml::Vec2 const &position, lml::Vec2 const &xbasis, lml::Vec2 const &ybasis, Font const *font, const char *str, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_texture(BuildState &state, lml::Vec2 const &position, lml::Rect2 const &rect, Vulkan::Texture const &texture, float layer = 0, lml::Color4 const &tint = { 1.0f, 1.0f, 1.0f, 1.0f });

    void push_scissor(BuildState &state, lml::Rect2 const &cliprect);

    void finalise(BuildState &state);

  public:

    CommandLump const *release() { return m_commandlump.release(); }

  private:

    unique_resource<CommandLump> m_commandlump;
};
