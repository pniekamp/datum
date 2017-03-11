#version 440 core
#include "bound.glsl"
#include "camera.glsl"
#include "gbuffer.glsl"
#include "transform.glsl"
#include "lighting.glsl"

layout(constant_id = 46) const uint ShadowSlices = 4;
layout(constant_id = 29) const uint MaxPointLights = 256;
layout(constant_id = 31) const uint MaxEnvironments = 6;

layout(std430, set=0, binding=0, row_major) buffer SceneSet 
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
  
  MainLight mainlight;

  float splits[4];
  mat4 shadowview[4];
  
  uint environmentcount;
  Environment environments[MaxEnvironments];

  uint pointlightcount;
  PointLight pointlights[MaxPointLights];

} scene;

layout(std430, set=1, binding=0, row_major) buffer MaterialSet 
{
  vec4 color;
  float roughness;
  float reflectivity;
  float emissive;

} material;

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform sampler2DArray specularmap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

layout(set=0, binding=4) uniform sampler2D depthmap;
layout(set=0, binding=6) uniform sampler2DArrayShadow shadowmap;

layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in mat3 tbnworld;

layout(location=0) out vec4 fragcolor;

///////////////////////// mainlight_shadow //////////////////////////////////
float mainlight_shadow(MainLight light, vec3 position, vec3 normal)
{
  const float bias[ShadowSlices] = { 0.05, 0.06, 0.10, 0.25 };
  const float spread[ShadowSlices] = { 1.5, 1.2, 1.0, 0.2 };

  for(uint i = 0; i < ShadowSlices; ++i)
  {
    vec3 shadowpos = position + bias[i] * normal;
    vec4 shadowspace = scene.shadowview[i] * vec4(shadowpos, 1);
    
    vec4 texel = vec4(0.5 * shadowspace.xy + 0.5, i, shadowspace.z);

    if (texel.x > 0.0 && texel.x < 1.0 && texel.y > 0.0 && texel.y < 1.0 && texel.w > 0.0 && texel.w < 1.0)
    { 
      return shadow_intensity(scene.shadowview[i], shadowpos, i, shadowmap, spread[i]);
    }
  }
  
  return 1.0;
}

///////////////////////// main //////////////////////////////////////////////
void main()
{ 
  vec3 normal = normalize(tbnworld * (2 * texture(normalmap, vec3(texcoord, 0)).xyz - 1));
  vec3 eyevec = normalize(scene.camera.position - position);

  vec4 color = texture(albedomap, vec3(texcoord, 0));

  vec4 rt0 = vec4(color.rgb * material.color.rgb, material.emissive);
  vec4 rt1 = texture(specularmap, vec3(texcoord, 0)) * vec4(0, material.reflectivity, 0, material.roughness);

  Material surface = unpack_material(rt0, rt1);

  float ambientintensity = 1.0;
  
  float mainlightshadow = mainlight_shadow(scene.mainlight, position, normal);
  
  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);

  env_light(diffuse, specular, surface, vec3(0.2), vec3(0), vec2(0), ambientintensity);

  main_light(diffuse, specular, scene.mainlight, normal, eyevec, surface, mainlightshadow);

  fragcolor = vec4(scene.camera.exposure * ((diffuse + surface.emissive) * surface.diffuse + specular), color.a * material.color.a);
}
