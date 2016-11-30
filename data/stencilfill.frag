#version 450 core

layout(std430, set=1, binding=0, row_major) buffer MaterialSet 
{
  vec4 color;
  
} material;

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(location=0) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  fragcolor = texture(albedomap, vec3(texcoord, 0)) * material.color;
}
