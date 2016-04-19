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

    void push_sprites(SpriteList const &sprites);

  private:

    PushBuffer m_buffer;
};
