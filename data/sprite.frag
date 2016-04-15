#version 450 core

layout(std140, set=1, binding=0) buffer MaterialSet 
{
  vec4 tint;
  vec4 texcoords;

} material;

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(location = 0) in vec3 texcoord;

layout(location = 0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  fragcolor = texture(albedomap, texcoord) * material.tint;
}
