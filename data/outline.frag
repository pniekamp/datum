#version 450 core

layout(std430, set=1, binding=0, row_major) buffer MaterialSet 
{
  vec4 color;

} material;

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(set=0, binding=4) uniform sampler2D depthmap;

layout(location=0) in vec2 texcoord;
layout(location=1) noperspective in vec4 fbocoord;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  if (fbocoord.x < 0 || fbocoord.x > 1 || fbocoord.y < 0 || fbocoord.y > 1)
    discard;

  float alpha = 1.0;
  
  if (texture(depthmap, fbocoord.st).r < fbocoord.z)
  {
    alpha = 0.2;
  }

  fragcolor = vec4(material.color.rgb * alpha, material.color.a) * texture(albedomap, vec3(texcoord, 0)).a;
}
