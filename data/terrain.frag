#version 440 core
#include "bound.glsl"
#include "camera.glsl"
#include "gbuffer.glsl"
#include "transform.glsl"
#include "lighting.glsl"

layout(constant_id = 52) const uint DecalMask = 0;

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  vec2 uvscale;
  uint layers;

} params;

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform sampler2DArray surfacemap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

layout(set=3, binding=0) uniform sampler2DArray blendmap;

layout(location=0) in vec3 position;
layout(location=1) in mat3 tbnworld;
layout(location=4) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;
layout(location=1) out vec4 fragspecular;
layout(location=2) out vec4 fragnormal;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 albedo = vec4(0);
  vec4 surface = vec4(0);
  vec3 normal = vec3(0);

  vec3 uv = vec3(texcoord * params.uvscale, 0);
  
  for(uint i = 0, end = params.layers/4; i < end; ++i)
  {  
    vec4 blend = texture(blendmap, vec3(texcoord, i));
    
    albedo += texture(albedomap, uv) * blend.r;
    surface += texture(surfacemap, uv) * blend.r;
    normal += texture(normalmap, uv).xyz * blend.r;
    uv.z += 1;

    albedo += texture(albedomap, uv) * blend.g;
    surface += texture(surfacemap, uv) * blend.g;
    normal += texture(normalmap, uv).xyz * blend.g;
    uv.z += 1;

    albedo += texture(albedomap, uv) * blend.b;
    surface += texture(surfacemap, uv) * blend.b;
    normal += texture(normalmap, uv).xyz * blend.b;
    uv.z += 1;

    albedo += texture(albedomap, uv) * blend.a;
    surface += texture(surfacemap, uv) * blend.a;
    normal += texture(normalmap, uv).xyz * blend.a;
    uv.z += 1;
  }
  
  if ((params.layers & 0x3) > 0)
  {
    vec4 blend = texture(blendmap, vec3(texcoord, params.layers/4));

    albedo += texture(albedomap, uv) * blend.r;
    surface += texture(surfacemap, uv) * blend.r;
    normal += texture(normalmap, uv).xyz * blend.r;
    uv.z += 1;

    if ((params.layers & 0x3) > 1)
    {
      albedo += texture(albedomap, uv) * blend.g;
      surface += texture(surfacemap, uv) * blend.g;
      normal += texture(normalmap, uv).xyz * blend.g;
      uv.z += 1;
    
      if ((params.layers & 0x3) > 2)
      {
        albedo += texture(albedomap, uv) * blend.b;
        surface += texture(surfacemap, uv) * blend.b;
        normal += texture(normalmap, uv).xyz * blend.b;
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
