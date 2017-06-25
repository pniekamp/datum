#version 440 core
#include "bound.glsl"
#include "camera.glsl"
#include "gbuffer.glsl"
#include "transform.glsl"
#include "lighting.glsl"

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;

} params;

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform sampler2DArray surfacemap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

layout(location=0) in vec3 position;
layout(location=1) in mat3 tbnworld;
layout(location=4) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;
layout(location=1) out vec4 fragspecular;
layout(location=2) out vec4 fragnormal;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 albedo = texture(albedomap, vec3(texcoord, 0));

  if (albedo.a < 0.5)
    discard;

  vec3 normal = normalize(tbnworld * (2 * texture(normalmap, vec3(texcoord, 0)).xyz - 1));

  vec4 surface = texture(surfacemap, vec3(texcoord, 0));

  Material material = make_material(albedo.rgb * params.color.rgb, params.emissive, params.metalness * surface.r, params.reflectivity * surface.g, params.roughness * surface.a);

  fragcolor = vec4(material.diffuse, params.emissive);
  fragspecular = vec4(material.specular, material.roughness);
  fragnormal = vec4(0.5 * normal + 0.5, 1);
}
