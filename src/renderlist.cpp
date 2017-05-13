//
// Datum - render list
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


///////////////////////// RenderList::push_geometry /////////////////////////
void RenderList::push_geometry(GeometryList const &geometry)
{
  if (geometry)
  {
    auto entry = m_buffer.push<Renderable::Geometry>();

    if (entry)
    {
      entry->commandlist = geometry.commandlist();
    }
  }
}


///////////////////////// RenderList::push_objects //////////////////////////
void RenderList::push_objects(ForwardList const &objects)
{
  if (objects)
  {
    auto entry = m_buffer.push<Renderable::Objects>();

    if (entry)
    {
      entry->commandlist = objects.commandlist();
    }
  }
}


///////////////////////// RenderList::push_casters //////////////////////////
void RenderList::push_casters(CasterList const &casters)
{
  if (casters)
  {
    auto entry = m_buffer.push<Renderable::Casters>();

    if (entry)
    {
      entry->commandlist = casters.commandlist();
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
      entry->lightlist = lights.lightlist();
    }
  }
}


///////////////////////// RenderList::push_environment //////////////////////
void RenderList::push_environment(Transform const &transform, Vec3 const &dimension, EnvMap const *envmap)
{
  if (envmap)
  {
    auto entry = m_buffer.push<Renderable::Environment>();

    if (entry)
    {
      entry->dimension = dimension;
      entry->transform = transform;
      entry->envmap = envmap;
    }
  }
}


///////////////////////// RenderList::push_sprites //////////////////////////
void RenderList::push_sprites(Rect2 const &viewport, SpriteList const &sprites)
{
  if (sprites)
  {
    auto entry = m_buffer.push<Renderable::Sprites>();

    if (entry)
    {
      sprites.viewport(viewport);

      entry->commandlist = sprites.commandlist();
    }
  }
}


///////////////////////// RenderList::push_sprites //////////////////////////
void RenderList::push_sprites(DatumPlatform::Viewport const &viewport, SpriteList const &sprites)
{
  push_sprites(Rect2(Vec2(viewport.x, viewport.y), Vec2(viewport.x + viewport.width, viewport.y + viewport.height)), sprites);
}


///////////////////////// RenderList::push_overlays /////////////////////////
void RenderList::push_overlays(OverlayList const &overlays)
{
  if (overlays)
  {
    auto entry = m_buffer.push<Renderable::Overlays>();

    if (entry)
    {
      entry->commandlist = overlays.commandlist();
    }
  }
}
