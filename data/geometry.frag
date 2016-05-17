#version 450 core

layout(std430, set=1, binding=0) buffer MaterialSet 
{
  vec3 albedocolor;
  vec3 specularintensity;
  float specularexponent;

} material;

layout(set=1, binding=1) uniform sampler2D albedomap;
layout(set=1, binding=2) uniform sampler2D specularmap;
layout(set=1, binding=3) uniform sampler2D normalmap;

layout(location=0) in vec2 texcoord;
layout(location=1) in mat3 tbnworld;

layout(location=0) out vec4 fragalbedo;
layout(location=1) out vec4 fragspecular;
layout(location=2) out vec4 fragnormals;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 albedo = texture(albedomap, texcoord);

  if (albedo.a < 0.5)
    discard;

  vec4 specular = texture(specularmap, texcoord);

  vec3 normal = normalize(tbnworld * (2.0 * texture(normalmap, texcoord).xyz - 1.0));

  fragalbedo = vec4(albedo.rgb * material.albedocolor, 1.0);
  fragspecular = vec4(material.specularintensity * specular.rgb, material.specularexponent / 1000.0);
  fragnormals  = vec4(0.5*normal+0.5, 1.0);
}
