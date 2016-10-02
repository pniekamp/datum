//
// Datum - model
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "model.h"
#include "assetpack.h"
#include "meshcomponent.h"
#include "debug.h"

using namespace std;
using namespace lml;


//|---------------------- Model ---------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Model::create /////////////////////////////////////
template<>
Scene::EntityId Scene::create<Model>(ResourceManager *resourcemanager)
{
  auto id = add_entity<Model>(this, resourcemanager, allocator<>());

  add_component<TransformComponent>(id, Transform::identity());

  get<Model>(id)->id = id;

  return id;
}


///////////////////////// Model::load ///////////////////////////////////////
template<>
Scene::EntityId Scene::load<Model>(DatumPlatform::PlatformInterface &platform, ResourceManager *resources, Asset const *asset)
{
  if (!asset)
    return EntityId{};

  asset_guard lock(resources->assets());

  auto model = get<Model>(create<Model>(resources));

  auto assets = resources->assets();

  void const *bits;

  while (!(bits = assets->request(platform, asset)))
    ;

  auto payload = reinterpret_cast<PackModelPayload const *>(bits);

  auto texturetable = PackModelPayload::texturetable(payload, asset->texturecount, asset->materialcount, asset->meshcount, asset->instancecount);
  auto materialtable = PackModelPayload::materialtable(payload, asset->texturecount, asset->materialcount, asset->meshcount, asset->instancecount);
  auto meshtable = PackModelPayload::meshtable(payload, asset->texturecount, asset->materialcount, asset->meshcount, asset->instancecount);
  auto instancetable = PackModelPayload::instancetable(payload, asset->texturecount, asset->materialcount, asset->meshcount, asset->instancecount);

  model->textures.resize(asset->texturecount);

  for(int i = 0; i < asset->texturecount; ++i)
  {
    switch (texturetable[i].type)
    {
      case PackModelPayload::Texture::nullmap:
        model->textures[i] = nullptr;
        break;

      case PackModelPayload::Texture::albedomap:
        model->textures[i] = resources->create<Texture>(assets->find(asset->id + texturetable[i].texture), Texture::Format::SRGBA);
        break;

      case PackModelPayload::Texture::specularmap:
        model->textures[i] = resources->create<Texture>(assets->find(asset->id + texturetable[i].texture), Texture::Format::RGBA);
        break;

      case PackModelPayload::Texture::normalmap:
        model->textures[i] = resources->create<Texture>(assets->find(asset->id + texturetable[i].texture), Texture::Format::RGBA);
        break;
    }
  }

  model->materials.resize(asset->materialcount);

  for(int i = 0; i < asset->materialcount; ++i)
  {
    auto color = Color3(materialtable[i].color[0], materialtable[i].color[1], materialtable[i].color[2]);

    auto metalness = materialtable[i].metalness;
    auto roughness = materialtable[i].roughness;
    auto reflectivity = materialtable[i].reflectivity;
    auto emissive = materialtable[i].emissive;

    auto albedomap = model->textures[materialtable[i].albedomap];
    auto specularmap = model->textures[materialtable[i].specularmap];
    auto normalmap = model->textures[materialtable[i].normalmap];

    model->materials[i] = resources->create<Material>(color, metalness, roughness, reflectivity, emissive, albedomap, specularmap, normalmap);
  }

  model->meshes.resize(asset->meshcount);

  for(int i = 0; i < asset->meshcount; ++i)
  {
    model->meshes[i] = resources->create<Mesh>(assets->find(asset->id + meshtable[i].mesh));
  }

  for(int i = 0; i < asset->instancecount; ++i)
  {
    auto transform =  Transform{ { instancetable[i].transform[0], instancetable[i].transform[1], instancetable[i].transform[2], instancetable[i].transform[3] }, { instancetable[i].transform[4], instancetable[i].transform[5], instancetable[i].transform[6], instancetable[i].transform[7] } };

    model->add_instance(transform, instancetable[i].mesh, instancetable[i].material, MeshComponent::Visible | MeshComponent::Static);
  }

  return model->id;
}


///////////////////////// Model::Constructor ////////////////////////////////
Model::Model(Scene *scene, ResourceManager *resourcemanager, StackAllocatorWithFreelist<> const &allocator)
  : textures(allocator),
    materials(allocator),
    meshes(allocator),
    dependants(allocator)
{
  m_scene = scene;
  m_resourcemanager = resourcemanager;
}


///////////////////////// Model::Destructor /////////////////////////////////
Model::~Model()
{
  for(auto &dependant : dependants)
  {
    if (m_scene->get(dependant))
      m_scene->destroy(dependant);
  }

  for(auto &mesh : meshes)
  {
    if (mesh)
      m_resourcemanager->release(mesh);
  }

  for(auto &material : materials)
  {
    if (material)
      m_resourcemanager->release(material);
  }

  for(auto &texture : textures)
  {
    if (texture)
      m_resourcemanager->release(texture);
  }
}


///////////////////////// Model::add_texture ////////////////////////////////
size_t Model::add_texture(Texture const *texture)
{
  textures.push_back(texture);

  return textures.size() - 1;
}


///////////////////////// Model::add_material ///////////////////////////////
size_t Model::add_material(Material const *material)
{
  materials.push_back(material);

  return materials.size() - 1;
}


///////////////////////// Model::add_mesh ///////////////////////////////////
size_t Model::add_mesh(Mesh const *mesh)
{
  meshes.push_back(mesh);

  return meshes.size() - 1;
}


///////////////////////// Model::add_instance ///////////////////////////////
Scene::EntityId Model::add_instance(Transform const &transform, size_t mesh, size_t material, long flags)
{
  auto instance = m_scene->create<Entity>();

  m_scene->add_component<TransformComponent>(instance, m_scene->get_component<TransformComponent>(id), transform);
  m_scene->add_component<MeshComponent>(instance, meshes[mesh], materials[material], flags);

  dependants.push_back(instance);

  return instance;
}

