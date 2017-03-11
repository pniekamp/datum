#version 440 core

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  fragcolor = vec4(1, 1, 1, 1);
}
