//
// Datum - render list
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "datum/renderer.h"

//|---------------------- RenderList ----------------------------------------
//|--------------------------------------------------------------------------

class RenderList
{
  public:

    typedef StackAllocator<> allocator_type;

    RenderList(allocator_type const &allocator, std::size_t slabsize);

    RenderList(PushBuffer const &other) = delete;

  public:

    operator PushBuffer &() { return m_buffer; }
    operator PushBuffer const &() const { return m_buffer; }

    void push_geometry(GeometryList const &geometry);

    void push_objects(ForwardList const &objects);

    void push_casters(CasterList const &casters);

    void push_lights(LightList const &lights);
    void push_environment(lml::Transform const &transform, lml::Vec3 const &dimension, EnvMap const *envmap);

    void push_sprites(lml::Rect2 const &viewport, SpriteList const &sprites);
    void push_sprites(DatumPlatform::Viewport const &viewport, SpriteList const &sprites);

    void push_overlays(OverlayList const &overlays);

  private:

    PushBuffer m_buffer;
};
