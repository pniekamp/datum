
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
Bound3 make_bound(Bound2 tile, float nz, float fz)
{
  Bound3 result;
  
  float minx = min(tile.min.x * nz, tile.min.x * fz);
  float maxx = max(tile.max.x * nz, tile.max.x * fz);

  float miny = min(tile.min.y * nz, tile.min.y * fz);
  float maxy = max(tile.max.y * nz, tile.max.y * fz);

  result.centre = 0.5*vec3(minx + maxx, miny + maxy, -(nz + fz));
  result.halfdim = 0.5*vec3(maxx - minx, maxy - miny, fz - nz);

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


///////////////////////// spotlight_bound ///////////////////////////////////
Bound3 spotlight_bound(vec3 origin, vec3 direction, float range, float cutoff)
{
	Bound3 result;
  
  if (cutoff > 0.707106)
	{
		result.centre = origin + range / (2 * cutoff) * direction;
    result.halfdim = vec3(range / (2 * cutoff));
	}
	else if (cutoff > 0)
	{
		result.centre = origin + cutoff * range * direction;
    result.halfdim = vec3(sqrt((1 - cutoff*cutoff)) * range);
	}
	else
	{
		result.centre = origin;
    result.halfdim = vec3(range);
	}  

	return result;
}