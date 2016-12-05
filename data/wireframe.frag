#version 450 core

layout(std430, set=1, binding=0, row_major) buffer MaterialSet 
{
  vec4 color;

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

  if (texture(depthmap, fbocoord.st).r < fbocoord.z - 1e-5)
    discard;

  //  float alpha = (gl_FrontFacing) ? 1.0 : 0.1;

  float d = min(min(edgedist[0], edgedist[1]), edgedist[2]);

  fragcolor = vec4(material.color.rgb, mix(0, material.color.a, exp2(-1.0*d*d)));
}
