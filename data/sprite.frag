#version 440 core

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;

} params;

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(location = 0) in vec3 texcoord0;
layout(location = 1) in vec3 texcoord1;
layout(location = 2) in float texblend;

layout(location = 0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  //fragcolor = texture(albedomap, texcoord0) * params.color;
  fragcolor = mix(texture(albedomap, texcoord0), texture(albedomap, texcoord1), texblend) * params.color;
}
