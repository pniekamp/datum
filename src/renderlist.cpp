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


///////////////////////// RenderList::push_sprites //////////////////////////
void RenderList::push_sprites(SpriteList const &sprites)
{
  if (sprites)
  {
    auto entry = m_buffer.push<Renderable::Sprites>();

    if (entry)
    {
      entry->spritelist = sprites;
    }
  }
}
