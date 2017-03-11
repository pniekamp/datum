#version 440 core

layout(std430, set=0, binding=0, row_major) buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;
  mat4 prevview;
  mat4 skyview;
  vec4 viewport;
  
} scene;

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

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec2 fbocoord = (gl_FragCoord.xy - scene.viewport.xy) / scene.viewport.zw;

  if (fbocoord.x < 0 || fbocoord.x > 1 || fbocoord.y < 0 || fbocoord.y > 1)
    discard;

  float depthfade = 1.0;
  
  if (texture(depthmap, fbocoord).r < gl_FragCoord.z)
  {
    depthfade = material.depthfade;
  
    if (depthfade == 0.0)
      discard;
  }
  
  fragcolor = texture(albedomap, vec3(material.texcoords.xy + material.texcoords.zw * fbocoord.st, 0)) * material.color * depthfade;
}
