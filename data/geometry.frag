#version 450 core

layout(std430, set=1, binding=0, row_major) buffer MaterialSet 
{
  vec4 color;
  float metalness;
  float smoothness;
  float reflectivity;

} material;

layout(set=1, binding=1) uniform sampler2D albedomap;
layout(set=1, binding=2) uniform sampler2D specularmap;
layout(set=1, binding=3) uniform sampler2D normalmap;

layout(location=0) in vec2 texcoord;
layout(location=1) in mat3 tbnworld;

layout(location=0) out vec4 fragrt0;
layout(location=1) out vec4 fragrt1;
layout(location=2) out vec4 fragnormal;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 albedo = texture(albedomap, texcoord);

  if (albedo.a < 0.5)
    discard;

  vec4 specular = texture(specularmap, texcoord);

  vec3 normal = normalize(tbnworld * (2.0 * texture(normalmap, texcoord).xyz - 1.0));

  fragrt0 = vec4(albedo.rgb * material.color.rgb, 0.0);
  fragrt1 = vec4(material.metalness * specular.r, material.reflectivity * specular.g, 0, material.smoothness * specular.a);
  fragnormal  = vec4(0.5*normal+0.5, 1.0);
}
