#version 440 core

layout(std430, set=1, binding=0, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  vec4 texcoords;

} material;

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(location = 0) in vec3 texcoord;

layout(location = 0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  fragcolor = texture(albedomap, texcoord) * material.color;
}
