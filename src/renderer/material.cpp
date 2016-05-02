//
// Datum - material
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "material.h"
#include "resource.h"
#include "assetpack.h"
#include "debug.h"

using namespace std;
using namespace lml;

enum MaterialFlags
{
  MaterialOwnsAlbedoMap = 0x01,
  MaterialOwnsSpecularMap = 0x02,
  MaterialOwnsNormalMap = 0x004,
};


//|---------------------- Material ------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// ResourceManager::create ///////////////////////////
template<>
Material const *ResourceManager::create<Material>(Asset const *asset)
{
  if (!asset)
    return nullptr;

  auto slot = acquire_slot(sizeof(Material));

  if (!slot)
    return nullptr;

  auto material = new(slot) Material;

  material->flags = 0;

  material->albedomap = nullptr;
  material->specularmap = nullptr;
  material->normalmap = nullptr;
  material->state = Material::State::Empty;

  set_slothandle(slot, asset);

  return material;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Material const *ResourceManager::create<Material>(Color3 albedocolor, Texture const *albedomap, Color3 specularintensity, float specularexponent, Texture const *specularmap, Texture const *normalmap)
{
  auto slot = acquire_slot(sizeof(Material));

  if (!slot)
    return nullptr;

  auto material = new(slot) Material;

  material->flags = 0;

  material->albedocolor = albedocolor;
  material->albedomap = albedomap;
  material->specularintensity = specularintensity;
  material->specularexponent = specularexponent;
  material->specularmap = specularmap;
  material->normalmap = normalmap;

  material->state = Material::State::Waiting;

  set_slothandle(slot, nullptr);

  return material;
}


///////////////////////// ResourceManager::request //////////////////////////
template<>
void ResourceManager::request<Material>(DatumPlatform::PlatformInterface &platform, Material const *material)
{
  assert(material);

  auto slot = const_cast<Material*>(material);

  Material::State empty = Material::State::Empty;

  if (slot->state.compare_exchange_strong(empty, Material::State::Loading))
  {
    auto asset = get_slothandle<Asset const *>(slot);

    if (asset)
    {
      auto bits = m_assets->request(platform, asset);

      if (bits)
      {
        auto material = reinterpret_cast<PackMaterialPayload const *>(bits);

        slot->albedocolor = Color3(material->albedocolor[0], material->albedocolor[1], material->albedocolor[2]);

        if (material->albedomap)
        {
          slot->albedomap = create<Texture>(assets()->find(asset->id + material->albedomap), Texture::Format::SRGBA);

          slot->flags |= MaterialOwnsAlbedoMap;
        }

        slot->specularintensity = Color3(material->specularintensity[0], material->specularintensity[1], material->specularintensity[2]);
        slot->specularexponent = material->specularexponent;

        if (material->specularmap)
        {
          slot->specularmap = create<Texture>(assets()->find(asset->id + material->specularmap), Texture::Format::SRGBA);

          slot->flags |= MaterialOwnsSpecularMap;
        }

        if (material->normalmap)
        {
          slot->normalmap = create<Texture>(assets()->find(asset->id + material->normalmap), Texture::Format::RGBA);

          slot->flags |= MaterialOwnsNormalMap;
        }

        slot->state = Material::State::Waiting;
      }
      else
        slot->state = Material::State::Empty;
    }
    else
      slot->state = Material::State::Empty;
  }

  Material::State waiting = Material::State::Waiting;

  if (slot->state.compare_exchange_strong(waiting, Material::State::Testing))
  {
    bool ready = true;

    if (material->albedomap)
    {
      request(platform, material->albedomap);

      ready &= material->albedomap->ready();
    }

    if (material->specularmap)
    {
      request(platform, material->specularmap);

      ready &= material->specularmap->ready();
    }

    if (material->normalmap)
    {
      request(platform, material->normalmap);

      ready &= material->normalmap->ready();
    }

    if (ready)
    {
      slot->state = Material::State::Ready;
    }
    else
      slot->state = Material::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Material>(Material const *material)
{
  assert(material);

  defer_destroy(material);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Material>(Material const *material)
{
  assert(material);

  auto slot = const_cast<Material*>(material);

  if (material->flags & MaterialOwnsAlbedoMap)
    destroy(material->albedomap);

  if (material->flags & MaterialOwnsSpecularMap)
    destroy(material->specularmap);

  if (material->flags & MaterialOwnsNormalMap)
    destroy(material->normalmap);

  material->~Material();

  release_slot(slot, sizeof(Material));
}

