#version 450 core

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

layout(location=0) in vec2 texcoord;
layout(location=1) in mat3 tbnworld;

layout(location=0) out vec4 fragrt0;
layout(location=1) out vec4 fragrt1;
layout(location=2) out vec4 fragnormal;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 albedo = texture(albedomap, vec3(texcoord, 0));

  if (albedo.a < 0.5)
    discard;

  vec4 specular = texture(specularmap, vec3(texcoord, 0));

  vec3 normal = normalize(tbnworld * (2 * texture(normalmap, vec3(texcoord, 0)).xyz - 1));

  fragrt0 = vec4(albedo.rgb * material.color.rgb, material.emissive / 10);
  fragrt1 = vec4(material.metalness * specular.r, material.reflectivity * specular.g, 0, material.roughness * specular.a);
  fragnormal  = vec4(0.5 * normal + 0.5, 1);
}
