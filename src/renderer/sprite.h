//
// Datum - sprite
//

//
// Copyright (c) 2015 Peter Niekamp
//

#pragma once

#include "resource.h"
#include "renderer.h"
#include "texture.h"


//|---------------------- Sprite --------------------------------------------
//|--------------------------------------------------------------------------

class Sprite
{
  public:
    friend Sprite const *ResourceManager::create<Sprite>(Asset const *asset);
    friend Sprite const *ResourceManager::create<Sprite>(Texture const *atlas, lml::Rect2 extent, lml::Vec2 align);

    bool ready() const { return atlas->ready(); }

    int flags;

    int width;
    int height;
    int layers;
    float aspect;
    lml::Vec2 align;
    lml::Vec4 extent;

    Texture const *atlas;

  private:
    Sprite() = default;
};

