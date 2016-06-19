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
Material const *ResourceManager::create<Material>(Color3 color, float metalness, float roughness, float reflectivity, float emissive, Texture const *albedomap, Texture const *specularmap, Texture const *normalmap)
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

  material->state = Material::State::Waiting;

  set_slothandle(slot, nullptr);

  return material;
}

template<>
Material const *ResourceManager::create<Material>(Color3 color, float emissive)
{
  return create<Material>(color, 0.0f, 1.0f, 0.5f, emissive, (Texture const *)nullptr, (Texture const *)nullptr, (Texture const *)nullptr);
}

template<>
Material const *ResourceManager::create<Material>(Color3 color, float metalness, float roughness)
{
  return create<Material>(color, metalness, roughness, 0.5f, 0.0f, (Texture const *)nullptr, (Texture const *)nullptr, (Texture const *)nullptr);
}

template<>
Material const *ResourceManager::create<Material>(Color3 color, float metalness, float roughness, float reflectivity)
{
  return create<Material>(color, metalness, roughness, reflectivity, 0.0f, (Texture const *)nullptr, (Texture const *)nullptr, (Texture const *)nullptr);
}

template<>
Material const *ResourceManager::create<Material>(Color3 color, float metalness, float roughness, float reflectivity, float emissive)
{
  return create<Material>(color, metalness, roughness, reflectivity, emissive, (Texture const *)nullptr, (Texture const *)nullptr, (Texture const *)nullptr);
}


///////////////////////// ResourceManager::update ///////////////////////////
template<>
void ResourceManager::update<Material>(Material const *material, Color3 color, float metalness, float roughness, float reflectivity, float emissive)
{
  assert(material);

  auto slot = const_cast<Material*>(material);

  slot->color = color;
  slot->metalness = metalness;
  slot->roughness = roughness;
  slot->reflectivity = reflectivity;
  slot->emissive = emissive;
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

        slot->color = Color3(material->color[0], material->color[1], material->color[2]);

        slot->metalness = material->metalness;
        slot->roughness = material->roughness;
        slot->reflectivity = material->reflectivity;
        slot->emissive = material->emissive;

        if (material->albedomap)
        {
          slot->albedomap = create<Texture>(assets()->find(asset->id + material->albedomap), Texture::Format::SRGBA);

          slot->flags |= MaterialOwnsAlbedoMap;
        }

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

