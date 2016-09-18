
//----------------------- GBuffer -------------------------------------------
//---------------------------------------------------------------------------
  
///////////////////////// project ///////////////////////////////////////////
vec2 project(mat4 proj, vec3 viewpos)
{
  vec4 projected = proj * vec4(viewpos, 1);
  
  return projected.xy / projected.w;
}


///////////////////////// view_depth ////////////////////////////////////////
float view_depth(mat4 proj, float depth)
{
  return proj[3][2] / (depth + proj[2][2]);
}


///////////////////////// view_normal ///////////////////////////////////////
vec3 view_normal(mat4 view, vec3 normal)
{
  return normalize((view * vec4(2 * normal - 1, 0)).xyz);
}


///////////////////////// view_position /////////////////////////////////////
vec3 view_position(mat4 proj, vec3 viewray, float depth)
{
  return viewray * view_depth(proj, depth);
}

vec3 view_position(mat4 proj, mat4 invproj, vec2 texcoord, float depth)
{
  return view_position(proj, vec3(invproj[0][0] * (2 * texcoord.x - 1), invproj[1][1] * (2 * texcoord.y - 1), -1), depth);
}

vec3 view_position(mat4 proj, mat4 invproj, ivec2 xy, ivec2 viewport, float depth)
{
  return view_position(proj, invproj, vec2(xy + 0.5)/viewport, depth);
}

vec3 view_position(mat4 view, vec3 position)
{
  return (view * vec4(position, 1)).xyz;
}


///////////////////////// world_normal //////////////////////////////////////
vec3 world_normal(vec3 normal)
{
  return normalize(2 * normal - 1);
}


///////////////////////// world_position ////////////////////////////////////
vec3 world_position(mat4 invview, mat4 proj, vec3 viewray, float depth)
{
  return (invview * vec4(view_position(proj, viewray, depth), 1)).xyz;
}

vec3 world_position(mat4 invview, mat4 proj, mat4 invproj, ivec2 xy, ivec2 viewport, float depth)
{
  return (invview * vec4(view_position(proj, invproj, xy, viewport, depth), 1)).xyz;
}

vec3 world_position(mat4 invview, mat4 proj, mat4 invproj, vec2 texcoord, float depth)
{
  return (invview * vec4(view_position(proj, invproj, texcoord, depth), 1)).xyz;
}
