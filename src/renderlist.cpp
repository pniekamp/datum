//
// Datum - render group
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "renderlist.h"
#include "debug.h"

using namespace std;
using namespace lml;
using namespace DatumPlatform;


//|---------------------- RenderList ----------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// RenderList::Constructor ///////////////////////////
RenderList::RenderList(allocator_type const &allocator, std::size_t slabsize)
  : m_buffer(allocator, slabsize)
{
}


///////////////////////// RenderList::push_meshes ///////////////////////////
void RenderList::push_meshes(MeshList const &meshes)
{
  if (meshes)
  {
    auto entry = m_buffer.push<Renderable::Meshes>();

    if (entry)
    {
      entry->commandlist = meshes.commandlist();
    }
  }
}


///////////////////////// RenderList::push_lights ///////////////////////////
void RenderList::push_lights(LightList const &lights)
{
  if (lights)
  {
    auto entry = m_buffer.push<Renderable::Lights>();

    if (entry)
    {
      entry->commandlist = lights.commandlist();
    }
  }
}


///////////////////////// RenderList::push_sprites //////////////////////////
void RenderList::push_sprites(lml::Rect2 const &viewport, SpriteList const &sprites)
{
  if (sprites)
  {
    auto entry = m_buffer.push<Renderable::Sprites>();

    if (entry)
    {
      entry->viewport = viewport;
      entry->commandlist = sprites.commandlist();
    }
  }
}


///////////////////////// RenderList::push_sprites //////////////////////////
void RenderList::push_sprites(DatumPlatform::Viewport const &viewport, SpriteList const &sprites)
{
  push_sprites(Rect2(Vec2(viewport.x, viewport.y), Vec2(viewport.x + viewport.width, viewport.y + viewport.height)), sprites);
}
