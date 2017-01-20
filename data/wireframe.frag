#version 450 core

layout(std430, set=1, binding=0, row_major) buffer MaterialSet 
{
  vec4 color;
  float depthfade;

} material;

layout(set=0, binding=4) uniform sampler2D depthmap;

layout(location=0) noperspective in vec4 fbocoord;
layout(location=1) noperspective in vec3 edgedist;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  if (fbocoord.x < 0 || fbocoord.x > 1 || fbocoord.y < 0 || fbocoord.y > 1)
    discard;

  float depthfade = 1.0;
  
  if (texture(depthmap, fbocoord.st).r < fbocoord.z - 1e-5)
  {
    depthfade = material.depthfade;
  }

  if (!gl_FrontFacing) 
  {
    depthfade *= 0.4;
  }

  float dist = min(min(edgedist[0], edgedist[1]), edgedist[2]);

  fragcolor = material.color * mix(0, material.color.a, exp2(-1.0*dist*dist)) * depthfade;
}
