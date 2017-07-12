#version 440 core

layout(set=0, binding=1) uniform sampler2D sourcemap;

out float gl_FragDepth;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  gl_FragDepth = texelFetch(sourcemap, ivec2(gl_FragCoord.xy), 0).r;
}
