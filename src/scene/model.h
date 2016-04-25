//
// Datum - model
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "entity.h"
#include "datum/math.h"
#include "datum/renderer.h"


//|---------------------- Model ---------------------------------------------
//|--------------------------------------------------------------------------

class Model : public Entity
{
  public:
    friend Scene::EntityId Scene::create<Model>(ResourceManager &resourcemanager);
    friend Scene::EntityId Scene::load<Model>(DatumPlatform::PlatformInterface &platform, ResourceManager *resources, Asset const *asset);

    size_t add_texture(Texture const *texture);
    size_t add_material(Material const *material);
    size_t add_mesh(Mesh const *mesh);

    Scene::EntityId add_instance(lml::Transform const &transform, size_t mesh, size_t material, long flags);

    std::vector<Texture const *, StackAllocatorWithFreelist<>> textures;
    std::vector<Material const *, StackAllocatorWithFreelist<>> materials;
    std::vector<Mesh const *, StackAllocatorWithFreelist<>> meshes;

    std::vector<Scene::EntityId, StackAllocatorWithFreelist<>> dependants;

  protected:
    Model(Scene *scene, ResourceManager *resources, StackAllocatorWithFreelist<> const &allocator);
    virtual ~Model();

    virtual std::type_info const &type() const override
    {
      return typeid(*this);
    }

  private:

    Scene::EntityId id;

    Scene *m_scene;
    ResourceManager *m_resourcemanager;

    friend class Scene;
};
