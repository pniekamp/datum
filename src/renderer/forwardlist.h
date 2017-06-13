//
// Datum - forward list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "renderer.h"
#include "resource.h"
#include "commandlist.h"
#include "mesh.h"
#include "material.h"
#include "particlesystem.h"
#include <utility>

//|---------------------- ForwardList ---------------------------------------
//|--------------------------------------------------------------------------

class ForwardList
{
  public:

    operator bool() const { return m_commandlist; }

    CommandList const *commandlist() const { return m_commandlist; }

  public:

    struct BuildState
    {
      RenderContext *context;
      ResourceManager *resources;

      CommandList::Descriptor materialset;

      CommandList::Descriptor modelset;

      CommandList *commandlist = nullptr;
    };

    bool begin(BuildState &state, RenderContext &context, ResourceManager &resources);

    void push_fogplane(BuildState &state, lml::Color4 const &color, lml::Plane const &plane = { { 0.0f, 1.0f, 0.0f } , -4.0f }, float density = 0.01f, float startdistance = 10.0f, float falloff = 0.5f);

    void push_translucent(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, float alpha = 1.0f);

    void push_particlesystem(BuildState &state, ParticleSystem const *particlesystem, ParticleSystem::Instance const *particles);
    void push_particlesystem(BuildState &state, lml::Transform const &transform, ParticleSystem const *particlesystem, ParticleSystem::Instance const *particles);

    void push_water(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, EnvMap const *envmap, lml::Vec2 const &flow, lml::Vec3 const &bumpscale = { 1.0f, 1.0f, 1.0f }, float alpha = 1.0f);
    void push_water(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Transform const &envtransform, SkyBox const *skybox, lml::Vec2 const &flow, lml::Vec3 const &bumpscale = { 1.0f, 1.0f, 1.0f }, float alpha = 1.0f);
    void push_water(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material, lml::Transform const &envtransform, lml::Vec3 const &envdimension, EnvMap const *envmap, lml::Vec2 const &flow, lml::Vec3 const &bumpscale = { 1.0f, 1.0f, 1.0f }, float alpha = 1.0f);

    void push_spotlight(BuildState &state, lml::Transform const &transform, Mesh const *mesh, lml::Vec3 const &scale, float cutoff, float range, lml::Color3 const &intensity, lml::Attenuation const &attenuation);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};
