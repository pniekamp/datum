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

layout(location = 0) in vec2 texcoord;

layout(location = 0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  fragcolor = vec4(texture(albedomap, texcoord).rgb * material.albedocolor, 1);
}
