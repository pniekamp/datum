#version 440 core

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(location=0) in vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 albedo = texture(albedomap, vec3(texcoord, 0));

  if (albedo.a < 0.5)
    discard;
}
