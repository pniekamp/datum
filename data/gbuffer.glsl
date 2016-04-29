
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

float view_depth(mat4 proj, ivec2 xy, sampler2D depthmap)
{
  return view_depth(proj, texelFetch(depthmap, xy, 0).z);
}

float view_depth(mat4 proj, vec2 texcoord, sampler2D depthmap)
{
  return view_depth(proj, texture(depthmap, texcoord).z);
}


///////////////////////// view_normal ///////////////////////////////////////
vec3 view_normal(mat4 view, vec3 normal)
{
  return (view * vec4(2.0 * normal - 1.0, 0.0)).xyz;
}

vec3 view_normal(mat4 view, ivec2 xy, sampler2D normalmap)
{
  return view_normal(view, texelFetch(normalmap, xy, 0).xyz);
}

vec3 view_normal(mat4 view, vec2 texcoord, sampler2D normalmap)
{
  return view_normal(view, texture(normalmap, texcoord).xyz);
}


///////////////////////// view_position /////////////////////////////////////
vec3 view_position(mat4 proj, vec3 viewray, float depth)
{
  return viewray * view_depth(proj, depth);
}

vec3 view_position(mat4 proj, vec3 viewray, vec2 texcoord, sampler2D depthmap)
{
  return viewray * view_depth(proj, texcoord, depthmap);
}

vec3 view_position(mat4 proj, mat4 invproj, vec2 texcoord, float depth)
{
  return view_position(proj, vec3(invproj[0][0] * (2.0 * texcoord.x - 1.0), invproj[1][1] * (2.0 * texcoord.y - 1.0), -1.0), depth);
}

vec3 view_position(mat4 proj, mat4 invproj, ivec2 xy, ivec2 viewport, float depth)
{
  return view_position(proj, invproj, vec2(xy + 0.5)/viewport, depth);
}

vec3 view_position(mat4 proj, mat4 invproj, vec2 texcoord, sampler2D depthmap)
{
  return view_position(proj, invproj, texcoord, texture(depthmap, texcoord).z);
}

vec3 view_position(mat4 proj, mat4 invproj, ivec2 xy, ivec2 viewport, sampler2D depthmap)
{
  return view_position(proj, invproj, xy, viewport, texelFetch(depthmap, xy, 0).z);
}

vec3 view_position(mat4 view, vec3 world)
{
  return (view * vec4(world, 1)).xyz;
}


///////////////////////// world_normal //////////////////////////////////////
vec3 world_normal(ivec2 xy, sampler2D normalmap)
{
  return 2.0 * texelFetch(normalmap, xy, 0).xyz - 1.0;
}

vec3 world_normal(vec2 texcoord, sampler2D normalmap)
{
  return 2.0 * texture(normalmap, texcoord).xyz - 1.0;
}


///////////////////////// world_position ////////////////////////////////////
vec3 world_position(mat4 invview, mat4 proj, vec3 viewray, float depth)
{
  return (invview * vec4(view_position(proj, viewray, depth), 1)).xyz;
}

vec3 world_position(mat4 invview, mat4 proj, vec3 viewray, vec2 texcoord, sampler2D depthmap)
{
  return (invview * vec4(view_position(proj, viewray, texcoord, depthmap), 1)).xyz;
}

vec3 world_position(mat4 invview, mat4 proj, mat4 invproj, vec2 texcoord, float depth)
{
  return (invview * vec4(view_position(proj, invproj, texcoord, depth), 1)).xyz;
}

vec3 world_position(mat4 invview, mat4 proj, mat4 invproj, ivec2 xy, ivec2 viewport, float depth)
{
  return (invview * vec4(view_position(proj, invproj, xy, viewport, depth), 1)).xyz;
}

vec3 world_position(mat4 invview, mat4 proj, mat4 invproj, vec2 texcoord, sampler2D depthmap)
{
  return (invview * vec4(view_position(proj, invproj, texcoord, depthmap), 1)).xyz;
}

vec3 world_position(mat4 invview, mat4 proj, mat4 invproj, ivec2 xy, ivec2 viewport, sampler2D depthmap)
{
  return (invview * vec4(view_position(proj, invproj, xy, viewport, depthmap), 1)).xyz;
}