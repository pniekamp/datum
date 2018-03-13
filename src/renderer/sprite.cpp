//
// Datum - sprite
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "sprite.h"
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
  sprite->atlas = nullptr;
  sprite->asset = asset;
  sprite->state = Sprite::State::Empty;

  return sprite;
}

template<>
Sprite const *ResourceManager::create<Sprite>(Asset const *asset)
{
  return create<Sprite>(asset, Vec2(0.5, 0.5));
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Sprite const *ResourceManager::create<Sprite>(Texture const *atlas, Rect2 region, Vec2 align)
{
  auto slot = acquire_slot(sizeof(Sprite));

  if (!slot)
    return nullptr;

  auto sprite = new(slot) Sprite;

  sprite->flags = 0;
  sprite->width = (int)((region.max.x - region.min.x) * atlas->width);
  sprite->height = (int)((region.max.y - region.min.y) * atlas->height);
  sprite->layers = atlas->layers;
  sprite->aspect = (float)sprite->width / (float)sprite->height;
  sprite->align = align;
  sprite->extent = Vec4(region.min.x, region.min.y, region.max.x - region.min.x, region.max.y - region.min.y);
  sprite->atlas = atlas;
  sprite->asset = nullptr;
  sprite->state = Sprite::State::Waiting;

  if (atlas->ready())
    sprite->state = Sprite::State::Ready;

  return sprite;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<Sprite>(DatumPlatform::PlatformInterface &platform, Sprite const *sprite)
{
  assert(sprite);

  auto slot = const_cast<Sprite*>(sprite);

  Sprite::State empty = Sprite::State::Empty;

  if (slot->state.compare_exchange_strong(empty, Sprite::State::Loading))
  {
    if (auto asset = slot->asset)
    {
      slot->atlas = create<Texture>(asset, Texture::Format::SRGBA);

      slot->flags |= SpriteOwnsAtlas;
    }

    slot->state = (slot->atlas) ? Sprite::State::Waiting : Sprite::State::Empty;
  }

  Sprite::State waiting = Sprite::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, Sprite::State::Testing))
  {
    request(platform, slot->atlas);

    slot->state = (slot->atlas->ready()) ? Sprite::State::Ready : Sprite::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Sprite>(Sprite const *sprite)
{
  defer_destroy(sprite);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Sprite>(Sprite const *sprite)
{
  if (sprite)
  {
    if (sprite->flags & SpriteOwnsAtlas)
      destroy(sprite->atlas);

    sprite->~Sprite();

    release_slot(const_cast<Sprite*>(sprite), sizeof(Sprite));
  }
}
