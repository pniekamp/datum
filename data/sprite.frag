#version 440 core

layout(set=0, binding=1) uniform sampler repeatsampler;
layout(set=0, binding=2) uniform sampler clampedsampler;

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;

} params;

layout(set=1, binding=1) uniform texture2DArray albedomap;

layout(location = 0) in vec3 texcoord0;
layout(location = 1) in vec3 texcoord1;
layout(location = 2) in float texblend;

layout(location = 0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  //fragcolor = texture(sampler2DArray(albedomap, clampedsampler), texcoord0) * params.color;
  fragcolor = mix(texture(sampler2DArray(albedomap, clampedsampler), texcoord0), texture(sampler2DArray(albedomap, clampedsampler), texcoord1), texblend) * params.color;
}
