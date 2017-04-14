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
  MaterialOwnsNormalMap = 0x04,
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
  material->asset = asset;
  material->state = Material::State::Empty;

  return material;
}


///////////////////////// ResourceManager::create ///////////////////////////
template<>
Material const *ResourceManager::create<Material>(Color4 color, float metalness, float roughness, float reflectivity, float emissive, Texture const *albedomap, Texture const *specularmap, Texture const *normalmap)
{
  auto slot = acquire_slot(sizeof(Material));

  if (!slot)
    return nullptr;

  auto material = new(slot) Material;

  material->flags = 0;

  material->color = color;
  material->albedomap = albedomap;
  material->metalness = metalness;
  material->roughness = roughness;
  material->reflectivity = reflectivity;
  material->emissive = emissive;
  material->specularmap = specularmap;
  material->normalmap = normalmap;
  material->asset = nullptr;
  material->state = Material::State::Waiting;

  if ((!albedomap || albedomap->ready()) && (!specularmap || specularmap->ready()) && (!normalmap || normalmap->ready()))
    material->state = Material::State::Ready;

  return material;
}

template<>
Material const *ResourceManager::create<Material>(Color4 color, float emissive)
{
  return create<Material>(color, 0.0f, 1.0f, 0.5f, emissive, (Texture const *)nullptr, (Texture const *)nullptr, (Texture const *)nullptr);
}

template<>
Material const *ResourceManager::create<Material>(Color4 color, float metalness, float roughness)
{
  return create<Material>(color, metalness, roughness, 0.5f, 0.0f, (Texture const *)nullptr, (Texture const *)nullptr, (Texture const *)nullptr);
}

template<>
Material const *ResourceManager::create<Material>(Color4 color, float metalness, float roughness, float reflectivity)
{
  return create<Material>(color, metalness, roughness, reflectivity, 0.0f, (Texture const *)nullptr, (Texture const *)nullptr, (Texture const *)nullptr);
}

template<>
Material const *ResourceManager::create<Material>(Color4 color, float metalness, float roughness, float reflectivity, float emissive)
{
  return create<Material>(color, metalness, roughness, reflectivity, emissive, (Texture const *)nullptr, (Texture const *)nullptr, (Texture const *)nullptr);
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Material>(Material const *material, Color4 color, float metalness, float roughness, float reflectivity, float emissive)
{
  assert(material);

  auto slot = const_cast<Material*>(material);

  slot->color = color;
  slot->metalness = metalness;
  slot->roughness = roughness;
  slot->reflectivity = reflectivity;
  slot->emissive = emissive;
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Material>(Material const *material, Color4 color, float metalness, float roughness, float reflectivity, float emissive, Texture const *albedomap, Texture const *specularmap, Texture const *normalmap)
{
  assert(material);
  assert((material->flags & (MaterialOwnsAlbedoMap | MaterialOwnsSpecularMap | MaterialOwnsNormalMap)) == 0);
  assert(!albedomap || albedomap->ready());
  assert(!specularmap || specularmap->ready());
  assert(!normalmap || normalmap->ready());

  auto slot = const_cast<Material*>(material);

  slot->color = color;
  slot->metalness = metalness;
  slot->roughness = roughness;
  slot->reflectivity = reflectivity;
  slot->emissive = emissive;
  slot->albedomap = albedomap;
  slot->specularmap = specularmap;
  slot->normalmap = normalmap;
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
    if (auto asset = slot->asset)
    {
      assert(assets()->barriercount != 0);

      if (auto bits = m_assets->request(platform, asset))
      {
        auto payload = reinterpret_cast<PackMaterialPayload const *>(bits);

        slot->color = Color4(payload->color[0], payload->color[1], payload->color[2], payload->color[3]);

        slot->metalness = payload->metalness;
        slot->roughness = payload->roughness;
        slot->reflectivity = payload->reflectivity;
        slot->emissive = payload->emissive;

        if (payload->albedomap && !slot->albedomap)
        {
          slot->albedomap = create<Texture>(assets()->find(asset->id + payload->albedomap), Texture::Format::SRGBA);

          slot->flags |= MaterialOwnsAlbedoMap;
        }

        if (payload->specularmap && !slot->specularmap)
        {
          slot->specularmap = create<Texture>(assets()->find(asset->id + payload->specularmap), Texture::Format::SRGBA);

          slot->flags |= MaterialOwnsSpecularMap;
        }

        if (payload->normalmap && !slot->normalmap)
        {
          slot->normalmap = create<Texture>(assets()->find(asset->id + payload->normalmap), Texture::Format::RGBA);

          slot->flags |= MaterialOwnsNormalMap;
        }

        slot->state = ((!payload->albedomap || slot->albedomap) && (!payload->specularmap || slot->specularmap) && (!payload->normalmap || slot->normalmap)) ? Material::State::Waiting : Material::State::Empty;
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

    if (slot->albedomap)
    {
      request(platform, slot->albedomap);

      ready &= slot->albedomap->ready();
    }

    if (slot->specularmap)
    {
      request(platform, slot->specularmap);

      ready &= slot->specularmap->ready();
    }

    if (slot->normalmap)
    {
      request(platform, slot->normalmap);

      ready &= slot->normalmap->ready();
    }

    slot->state = (ready) ? Material::State::Ready : Material::State::Waiting;
  }
}


///////////////////////// ResourceManager::release //////////////////////////
template<>
void ResourceManager::release<Material>(Material const *material)
{
  defer_destroy(material);
}


///////////////////////// ResourceManager::destroy //////////////////////////
template<>
void ResourceManager::destroy<Material>(Material const *material)
{
  if (material)
  {
    if (material->flags & MaterialOwnsAlbedoMap)
      destroy(material->albedomap);

    if (material->flags & MaterialOwnsSpecularMap)
      destroy(material->specularmap);

    if (material->flags & MaterialOwnsNormalMap)
      destroy(material->normalmap);

    material->~Material();

    release_slot(const_cast<Material*>(material), sizeof(Material));
  }
}

