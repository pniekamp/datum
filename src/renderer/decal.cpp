//
// Datum - decal
//

//
// Copyright (c) 2015 Peter Niekamp
//

#include "decal.h"
#include "assetpack.h"
#include "debug.h"

using namespace std;
using namespace lml;

enum DecalFlags
{
  DecalOwnsMaterial = 0x01,
};

//|---------------------- Decal ---------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
Decal const *ResourceManager::create<Decal>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(Decal));

  if (!slot)
    return nullptr;

  auto decal = new(slot) Decal;

  decal->flags = 0;
  decal->extent = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
  decal->material = nullptr;
  decal->asset = asset;
  decal->state = Decal::State::Empty;

  return decal;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Decal const *ResourceManager::create<Decal>(Material const *material, Rect2 region)
{
  auto slot = acquire_slot(sizeof(Decal));

  if (!slot)
    return nullptr;

  auto decal = new(slot) Decal;

  decal->flags = 0;
  decal->extent = Vec4(region.min.x, region.min.y, region.max.x - region.min.x, region.max.y - region.min.y);
  decal->material = material;
  decal->asset = nullptr;
  decal->state = Decal::State::Waiting;

  if (material->ready())
    decal->state = Decal::State::Ready;

  return decal;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<Decal>(DatumPlatform::PlatformInterface &platform, Decal const *decal)
{
  assert(decal);

  auto slot = const_cast<Decal*>(decal);

  Decal::State empty = Decal::State::Empty;

  if (slot->state.compare_exchange_strong(empty, Decal::State::Loading))
  {
    if (auto asset = slot->asset)
    {
      slot->material = create<Material>(asset);

      slot->flags |= DecalOwnsMaterial;
    }

    slot->state = (slot->material) ? Decal::State::Waiting : Decal::State::Empty;
  }

  Decal::State waiting = Decal::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, Decal::State::Testing))
  {
    request(platform, slot->material);

    slot->state = (slot->material->ready()) ? Decal::State::Ready : Decal::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Decal>(Decal const *decal)
{
  defer_destroy(decal);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Decal>(Decal const *decal)
{
  if (decal)
  {
    if (decal->flags & DecalOwnsMaterial)
      destroy(decal->material);

    decal->~Decal();

    release_slot(const_cast<Decal*>(decal), sizeof(Decal));
  }
}
