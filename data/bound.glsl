
//----------------------- Bound ---------------------------------------------
//---------------------------------------------------------------------------

struct Bound2
{
  vec2 min;
  vec2 max;
};

struct Bound3
{
  vec3 centre;
  vec3 halfdim;
};


///////////////////////// make_bound ////////////////////////////////////////
Bound2 make_bound(vec2 min, vec2 max)
{
  Bound2 result;
  
  result.min = min;
  result.max = max;

  return result;
}


///////////////////////// make_bound ////////////////////////////////////////
Bound3 make_bound(vec3 min, vec3 max)
{
  Bound3 result;
  
  result.centre = 0.5 * (min + max);
  result.halfdim = 0.5 * (max - min);

  return result;
}


///////////////////////// make_bound ////////////////////////////////////////
Bound3 make_bound(Bound2 tile, float minz, float maxz)
{
  Bound3 result;
  
  float minx = min(tile.min.x * -minz, tile.min.x * -maxz);
  float maxx = max(tile.max.x * -minz, tile.max.x * -maxz);

  float miny = min(tile.min.y * -minz, tile.min.y * -maxz);
  float maxy = max(tile.max.y * -minz, tile.max.y * -maxz);

  result.centre = 0.5*vec3(minx + maxx, miny + maxy, minz + maxz);
  result.halfdim = 0.5*vec3(maxx - minx, maxy - miny, minz - maxz);

  return result;
}


///////////////////////// intersects ////////////////////////////////////////
bool intersects(Bound3 bound, vec3 spherecentre, float sphereradius)
{
  vec3 delta = max(vec3(0),  abs(bound.centre - spherecentre) - bound.halfdim);
  
  return dot(delta, delta) <= sphereradius * sphereradius;
}


///////////////////////// intersections /////////////////////////////////////
vec2 intersections(vec3 origin, vec3 direction, vec3 halfdim)
{ 
  vec3 invdirection = 1 / direction;

  float txmin, txmax, tymin, tymax, tzmin, tzmax;
  txmin = (((invdirection.x < 0) ? +halfdim.x : -halfdim.x) - origin.x) * invdirection.x;
  txmax = (((invdirection.x < 0) ? -halfdim.x : +halfdim.x) - origin.x) * invdirection.x;
  tymin = (((invdirection.y < 0) ? +halfdim.y : -halfdim.y) - origin.y) * invdirection.y;
  tymax = (((invdirection.y < 0) ? -halfdim.y : +halfdim.y) - origin.y) * invdirection.y;
  tzmin = (((invdirection.z < 0) ? +halfdim.z : -halfdim.z) - origin.z) * invdirection.z;
  tzmax = (((invdirection.z < 0) ? -halfdim.z : +halfdim.z) - origin.z) * invdirection.z;
  
  return vec2(max(max(txmin, tymin), tzmin), min(min(txmax, tymax), tzmax));
}
