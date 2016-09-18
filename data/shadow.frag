#version 450 core

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(location=0) in vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  if (texture(albedomap, vec3(texcoord, 0)).a < 0.5)
    discard;
}
