#version 440 core
#include "bound.inc"
#include "transform.inc"
#include "camera.inc"
#include "gbuffer.inc"
#include "lighting.inc"

layout(constant_id = 52) const uint DecalMask = 0;

layout(set=0, binding=1) uniform sampler repeatsampler;
layout(set=0, binding=2) uniform sampler clampedsampler;

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;

} params;

layout(set=1, binding=1) uniform texture2DArray albedomap;
layout(set=1, binding=2) uniform texture2DArray surfacemap;
layout(set=1, binding=3) uniform texture2DArray normalmap;

layout(set=3, binding=2) uniform texture2DArray blendmap;

layout(location=0) in vec3 position;
layout(location=1) in mat3 tbnworld;
layout(location=4) in vec2 texcoord;
layout(location=5) in vec2 tilecoord;
layout(location=6) flat in uint layers;

layout(location=0) out vec4 fragcolor;
layout(location=1) out vec4 fragspecular;
layout(location=2) out vec4 fragnormal;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 albedo = vec4(0);
  vec4 surface = vec4(0);
  vec3 normal = vec3(0);

  vec3 uv = vec3(tilecoord, 0);
  
  for(uint i = 0, end = layers/4; i < end; ++i)
  {  
    vec4 blend = texture(sampler2DArray(blendmap, clampedsampler), vec3(texcoord, i));
    
    albedo += texture(sampler2DArray(albedomap, repeatsampler), uv) * blend.r;
    surface += texture(sampler2DArray(surfacemap, repeatsampler), uv) * blend.r;
    normal += texture(sampler2DArray(normalmap, repeatsampler), uv).xyz * blend.r;
    uv.z += 1;

    albedo += texture(sampler2DArray(albedomap, repeatsampler), uv) * blend.g;
    surface += texture(sampler2DArray(surfacemap, repeatsampler), uv) * blend.g;
    normal += texture(sampler2DArray(normalmap, repeatsampler), uv).xyz * blend.g;
    uv.z += 1;

    albedo += texture(sampler2DArray(albedomap, repeatsampler), uv) * blend.b;
    surface += texture(sampler2DArray(surfacemap, repeatsampler), uv) * blend.b;
    normal += texture(sampler2DArray(normalmap, repeatsampler), uv).xyz * blend.b;
    uv.z += 1;

    albedo += texture(sampler2DArray(albedomap, repeatsampler), uv) * blend.a;
    surface += texture(sampler2DArray(surfacemap, repeatsampler), uv) * blend.a;
    normal += texture(sampler2DArray(normalmap, repeatsampler), uv).xyz * blend.a;
    uv.z += 1;
  }
  
  if ((layers & 0x3) > 0)
  {
    vec4 blend = texture(sampler2DArray(blendmap, clampedsampler), vec3(texcoord, layers/4));

    albedo += texture(sampler2DArray(albedomap, repeatsampler), uv) * blend.r;
    surface += texture(sampler2DArray(surfacemap, repeatsampler), uv) * blend.r;
    normal += texture(sampler2DArray(normalmap, repeatsampler), uv).xyz * blend.r;
    uv.z += 1;

    if ((layers & 0x3) > 1)
    {
      albedo += texture(sampler2DArray(albedomap, repeatsampler), uv) * blend.g;
      surface += texture(sampler2DArray(surfacemap, repeatsampler), uv) * blend.g;
      normal += texture(sampler2DArray(normalmap, repeatsampler), uv).xyz * blend.g;
      uv.z += 1;
    
      if ((layers & 0x3) > 2)
      {
        albedo += texture(sampler2DArray(albedomap, repeatsampler), uv) * blend.b;
        surface += texture(sampler2DArray(surfacemap, repeatsampler), uv) * blend.b;
        normal += texture(sampler2DArray(normalmap, repeatsampler), uv).xyz * blend.b;
        uv.z += 1;
      }
    }    
  }

  normal = normalize(tbnworld * (2 * normal.xyz - 1));

  Material material = make_material(albedo.rgb * params.color.rgb, 0, surface.r, surface.g, surface.a);

  fragcolor = vec4(material.diffuse, 0);
  fragspecular = vec4(material.specular, material.roughness);
  fragnormal = vec4(0.5 * normal + 0.5, DecalMask/3.0+0.01);
}
