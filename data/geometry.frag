#version 440 core
#include "bound.glsl"
#include "transform.glsl"
#include "camera.glsl"
#include "gbuffer.glsl"
#include "lighting.glsl"

layout(constant_id = 31) const bool CutOut = true;
layout(constant_id = 52) const uint DecalMask = 0;

layout(set=0, binding=1) uniform sampler repeatsampler;
layout(set=0, binding=2) uniform sampler clampedsampler;

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;

} params;

layout(set=1, binding=1) uniform texture2DArray albedomap;
layout(set=1, binding=2) uniform texture2DArray surfacemap;
layout(set=1, binding=3) uniform texture2DArray normalmap;

layout(location=0) in vec3 position;
layout(location=1) in mat3 tbnworld;
layout(location=4) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;
layout(location=1) out vec4 fragspecular;
layout(location=2) out vec4 fragnormal;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 albedo = texture(sampler2DArray(albedomap, repeatsampler), vec3(texcoord, 0));
  vec4 surface = texture(sampler2DArray(surfacemap, repeatsampler), vec3(texcoord, 0));
  
  vec3 normal = normalize(tbnworld * (2 * texture(sampler2DArray(normalmap, repeatsampler), vec3(texcoord, 0)).xyz - 1));

  if (CutOut)
  {
    if (albedo.a < 0.95)
      discard;
  }

  Material material = make_material(albedo.rgb * params.color.rgb, params.emissive, params.metalness * surface.r, params.reflectivity * surface.g, params.roughness * surface.a);

  fragcolor = vec4(material.diffuse, params.emissive);
  fragspecular = vec4(material.specular, material.roughness);
  fragnormal = vec4(0.5 * normal + 0.5, DecalMask/3.0+0.01);
}
