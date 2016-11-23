#version 450 core

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(location=0) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  if (texture(albedomap, vec3(texcoord, 0)).a < 0.05)
    discard;
    
  fragcolor = vec4(1, 1, 1, 1);
}
