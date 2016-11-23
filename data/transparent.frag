#version 450 core
#include "bound.glsl"
#include "camera.glsl"
#include "gbuffer.glsl"
#include "transform.glsl"
#include "lighting.glsl"

layout(constant_id = 46) const uint ShadowSlices = 4;
layout(constant_id = 29) const uint MaxPointLights = 256;
layout(constant_id = 31) const uint MaxEnvironments = 6;
layout(constant_id = 72) const uint MaxTileLights = 48;

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
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;

} material;

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform sampler2DArray specularmap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

layout(set=0, binding=6) uniform sampler2DArrayShadow shadowmap;

layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in mat3 tbnworld;

layout(location=0) out vec4 fragcolor;
layout(location=1) out vec4 fragrt1;
layout(location=2) out vec4 fragnormal;

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
      float weight = max(4 * max(max(abs(shadowspace.x), abs(shadowspace.y)) - 0.75, 0), 500 * max(shadowspace.z - 0.998, 0));

      return shadow_intensity(scene.shadowview[i], shadowpos, i, shadowmap, spread[i]);
    }
  }
  
  return 1.0;
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 rt0 = texture(albedomap, vec3(texcoord, 0)) * material.color;
  vec4 rt1 = texture(specularmap, vec3(texcoord, 0)) * vec4(0, material.reflectivity, 0, material.roughness);
 
  vec3 normal = normalize(tbnworld * (2 * texture(normalmap, vec3(texcoord, 0)).xyz - 1));
  vec3 eyevec = normalize(scene.camera.position - position);

  Material material = unpack_material(rt0, rt1);
  
  float mainlightshadow = mainlight_shadow(scene.mainlight, position, normal);
  
  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);

  env_light(diffuse, specular, material, vec3(1), vec3(0), vec2(0), 0.2);

  main_light(diffuse, specular, scene.mainlight, normal, eyevec, material, mainlightshadow);

  fragcolor = vec4(scene.camera.exposure * (diffuse * material.diffuse + specular), rt0.a);

  fragrt1 = rt1;
  fragnormal  = vec4(0.5 * normal + 0.5, 1);  
}