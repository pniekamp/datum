#version 450 core

layout(std430, set=1, binding=0, row_major) buffer MaterialSet 
{
  vec4 color;
  vec4 texcoords;
  float depthfade;
  float halfwidth;
  float overhang;
  
} material;

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(set=0, binding=4) uniform sampler2D depthmap;

layout(location=0) noperspective in vec4 fbocoord;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  if (fbocoord.x < 0 || fbocoord.x > 1 || fbocoord.y < 0 || fbocoord.y > 1)
    discard;

  float depthfade = 1.0;
  
  if (texture(depthmap, fbocoord.st).r < fbocoord.z)
  {
    depthfade = material.depthfade;
  
    if (depthfade == 0.0)
      discard;
  }
  
  fragcolor = texture(albedomap, vec3(material.texcoords.xy + material.texcoords.zw * fbocoord.st, 0)) * material.color * depthfade;
}
