#version 440 core

layout(constant_id = 31) const bool CutOut = true;

layout(set=0, binding=1) uniform sampler repeatsampler;
layout(set=0, binding=2) uniform sampler clampedsampler;

layout(set=1, binding=1) uniform texture2DArray albedomap;

layout(location=4) in vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  if (CutOut)
  {
    vec4 albedo = texture(sampler2DArray(albedomap, repeatsampler), vec3(texcoord, 0));

    if (albedo.a < 0.95)
      discard;
  }
}
