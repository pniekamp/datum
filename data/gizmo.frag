#version 450 core
#include "camera.glsl"
#include "transform.glsl"
#include "lighting.glsl"

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

layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in mat3 tbnworld;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 rt0 = texture(albedomap, vec3(texcoord, 0)) * material.color;
  vec4 rt1 = texture(specularmap, vec3(texcoord, 0)) * vec4(0, material.reflectivity, 0, material.roughness);
 
  vec3 normal = normalize(tbnworld * (2 * texture(normalmap, vec3(texcoord, 0)).xyz - 1));
  vec3 eyevec = normalize(scene.camera.position - position);

  Material material = unpack_material(rt0, rt1);

  MainLight mainlight = { -eyevec, vec3(1.0f, 0.945f, 0.985f) };
  
  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);

  env_light(diffuse, specular, material, vec3(1), vec3(0), vec2(0), 0.2);

  main_light(diffuse, specular, mainlight, normal, eyevec, material, 1);

  fragcolor = vec4(diffuse * material.diffuse + specular, rt0.a);
}