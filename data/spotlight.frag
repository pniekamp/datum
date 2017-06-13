#version 440 core
#include "camera.glsl"
#include "gbuffer.glsl"
#include "transform.glsl"
#include "lighting.glsl"

layout(std430, set=0, binding=0, row_major) readonly buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;
  mat4 prevview;
  mat4 skyview;
  vec4 viewport;
  
  Camera camera;
  
} scene;

layout(std430, set=1, binding=0, row_major) readonly buffer MaterialSet 
{
  vec3 position;
  vec3 intensity;
  vec4 attenuation;
  vec3 direction;
  float cutoff;
  
} light;

//layout(set=1, binding=1) uniform sampler2DArrayShadow shadowmap;

layout(set=0, binding=1) uniform sampler2D rt0map;
layout(set=0, binding=2) uniform sampler2D rt1map;
layout(set=0, binding=3) uniform sampler2D normalmap;
layout(set=0, binding=4) uniform sampler2D depthmap;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{ 
  ivec2 xy = ivec2(gl_FragCoord.xy);
  
  ivec2 viewport = textureSize(depthmap, 0).xy;
  
  float depth = texelFetch(depthmap, xy, 0).r;
  
  vec3 position = world_position(scene.invview, scene.proj, scene.invproj, xy, viewport, depth);

  float lightdistance = length(light.position - position);
  
  if (light.attenuation.w < lightdistance)
    discard;

  if (dot(light.direction, position - light.position) < light.cutoff * lightdistance)
    discard;
    
  vec3 normal = world_normal(texelFetch(normalmap, xy, 0).xyz);
  vec3 eyevec = normalize(scene.camera.position - position);

  Material material = unpack_material(texelFetch(rt0map, xy, 0), texelFetch(rt1map, xy, 0)); 

  SpotLight spotlight = { light.position, light.intensity, light.attenuation, light.direction, light.cutoff };

  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);
  
  spot_light(diffuse, specular, spotlight, position, normal, eyevec, material, 1.0);
  
  fragcolor = vec4(scene.camera.exposure * ((diffuse + material.emissive) * material.diffuse + specular), 0);
}
