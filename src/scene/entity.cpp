//
// Datum - entity
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include "entity.h"
#include "debug.h"

using namespace std;


//|---------------------- Entity --------------------------------------------
//|--------------------------------------------------------------------------

///////////////////////// Entity::Constructor ///////////////////////////////
Entity::Entity()
{
}


///////////////////////// Entity::Destructor ////////////////////////////////
Entity::~Entity()
{
}


///////////////////////// Entity::create ////////////////////////////////////
template<>
Scene::EntityId Scene::create<Entity>()
{
  return push_entity<Entity>();
}
