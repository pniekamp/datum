//
// Datum - sprite
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "sprite.h"
#include "resource.h"
#include "assetpack.h"
#include "debug.h"

using namespace std;
using namespace lml;

enum SpriteFlags
{
  SpriteOwnsAtlas = 0x01,
};

//|---------------------- Sprite --------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
Sprite const *ResourceManager::create<Sprite>(Asset const *asset, Vec2 align)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(Sprite));

  if (!slot)
    return nullptr;

  auto sprite = new(slot) Sprite;

  sprite->flags = 0;
  sprite->width = asset->width;
  sprite->height = asset->height;
  sprite->layers = asset->layers;
  sprite->aspect = (float)asset->width / (float)asset->height;
  sprite->align = align;
  sprite->extent = Vec4(0.0f, 0.0f, 1.0f, 1.0f);

  sprite->atlas = create<Texture>(asset, Texture::Format::SRGBA);

  sprite->flags |= SpriteOwnsAtlas;

  return sprite;
}

template<>
Sprite const *ResourceManager::create<Sprite>(Asset const *asset)
{
  return create<Sprite>(asset, Vec2(0.5, 1.0));
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Sprite const *ResourceManager::create<Sprite>(Texture const *atlas, Rect2 extent, Vec2 align)
{
  auto slot = acquire_slot(sizeof(Sprite));

  if (!slot)
    return nullptr;

  auto sprite = new(slot) Sprite;

  sprite->flags = 0;
  sprite->width = (extent.max.x - extent.min.x) * atlas->width;
  sprite->height = (extent.max.y - extent.min.y) * atlas->height;
  sprite->layers = atlas->layers;
  sprite->aspect = (float)sprite->width / (float)sprite->height;
  sprite->align = align;
  sprite->extent = Vec4(extent.min.x, extent.min.y, extent.max.x - extent.min.x, extent.max.y - extent.min.y);

  sprite->atlas = atlas;

  return sprite;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<Sprite>(DatumPlatform::PlatformInterface &platform, Sprite const *sprite)
{
  assert(sprite);
  assert(sprite->atlas);

  request(platform, sprite->atlas);
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Sprite>(Sprite const *sprite)
{
  assert(sprite);

  defer_destroy(sprite);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Sprite>(Sprite const *sprite)
{
  assert(sprite);

  if (sprite->flags & SpriteOwnsAtlas)
    destroy(sprite->atlas);

  sprite->~Sprite();

  release_slot(const_cast<Sprite*>(sprite), sizeof(Sprite));
}
