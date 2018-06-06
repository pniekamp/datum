#version 440 core

layout(set=0, binding=1) uniform sampler repeatsampler;
layout(set=0, binding=2) uniform sampler clampedsampler;

layout(set=1, binding=1) uniform texture2D sourcemap;

out float gl_FragDepth;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  gl_FragDepth = texelFetch(sampler2D(sourcemap, clampedsampler), ivec2(gl_FragCoord.xy), 0).r;
}
