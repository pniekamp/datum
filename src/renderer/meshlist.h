//
// Datum - mesh list
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include "renderer.h"
#include "commandlist.h"
#include "mesh.h"
#include "material.h"
#include <utility>


//|---------------------- MeshList ------------------------------------------
//|--------------------------------------------------------------------------

class MeshList
{
  public:

    operator CommandList const *() const { return m_commandlist; }

  public:

    struct BuildState
    {
      DatumPlatform::PlatformInterface *platform;

      RenderContext *context;
      ResourceManager *resources;

      VkDeviceSize materialoffset;
      CommandList::Descriptor materialset;

      VkDeviceSize modeloffset;
      CommandList::Descriptor modelset;

      CommandList *commandlist = nullptr;

      Mesh const *mesh;
      Material const *material;
    };

    bool begin(BuildState &state, DatumPlatform::PlatformInterface &platform, RenderContext &context, ResourceManager *resources);

    void push_material(BuildState &state, Material const *material);

    void push_mesh(BuildState &state, lml::Transform const &transform, Mesh const *mesh);

    void push_mesh(BuildState &state, lml::Transform const &transform, Mesh const *mesh, Material const *material);

    void finalise(BuildState &state);

  private:

    unique_resource<CommandList> m_commandlist;
};
