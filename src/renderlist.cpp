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
void RenderList::push_geometry(GeometryList const &geometrylist)
{
  if (geometrylist)
  {
    auto entry = m_buffer.push<Renderable::Geometry>();

    if (entry)
    {
      entry->prepasscommands = geometrylist.prepasscommands;
      entry->geometrycommands = geometrylist.geometrycommands;
    }
  }
}


///////////////////////// RenderList::push_forward //////////////////////////
void RenderList::push_forward(ForwardList const &forwardlist)
{
  if (forwardlist)
  {
    auto entry = m_buffer.push<Renderable::Forward>();

    if (entry)
    {
      entry->forwardcommands = forwardlist.forwardcommands;
    }
  }
}


///////////////////////// RenderList::push_casters //////////////////////////
void RenderList::push_casters(CasterList const &casterlist)
{
  if (casterlist)
  {
    auto entry = m_buffer.push<Renderable::Casters>();

    if (entry)
    {
      entry->castercommands = casterlist.castercommands;
    }
  }
}


///////////////////////// RenderList::push_lights ///////////////////////////
void RenderList::push_lights(LightList const &lightlist)
{
  if (lightlist)
  {
    auto entry = m_buffer.push<Renderable::Lights>();

    if (entry)
    {
      entry->lightlist = lightlist.lightlist;
    }
  }
}


///////////////////////// RenderList::push_overlays /////////////////////////
void RenderList::push_overlays(OverlayList const &overlaylist)
{
  if (overlaylist)
  {
    auto entry = m_buffer.push<Renderable::Overlays>();

    if (entry)
    {
      entry->overlaycommands = overlaylist.overlaycommands;
    }
  }
}


///////////////////////// RenderList::push_sprites //////////////////////////
void RenderList::push_sprites(SpriteList const &spritelist)
{
  if (spritelist)
  {
    auto entry = m_buffer.push<Renderable::Sprites>();

    if (entry)
    {
      entry->spritecommands = spritelist.spritecommands;
    }
  }
}
