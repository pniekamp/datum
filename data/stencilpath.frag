#version 440 core

layout(set=0, binding=0, std430, row_major) readonly buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;
  mat4 orthoview;
  mat4 prevview;
  mat4 skyview;
  vec4 viewport;
  
} scene;

layout(set=0, binding=1) uniform sampler repeatsampler;
layout(set=0, binding=2) uniform sampler clampedsampler;

layout(set=0, binding=9) uniform sampler2D depthmap;

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  vec4 texcoords;
  float depthfade;
  
} params;

layout(set=1, binding=1) uniform texture2DArray albedomap;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec2 fbocoord = (gl_FragCoord.xy - scene.viewport.xy) / scene.viewport.zw;

  if (fbocoord.x < 0 || fbocoord.x > 1 || fbocoord.y < 0 || fbocoord.y > 1)
    discard;

  float depthfade = 1.0;
  
  if (texture(depthmap, fbocoord).r > gl_FragCoord.z)
  {
    depthfade = params.depthfade;
  
    if (depthfade == 0.0)
      discard;
  }
  
  fragcolor = texture(sampler2DArray(albedomap, repeatsampler), vec3(params.texcoords.xy + params.texcoords.zw * fbocoord.st, 0)) * params.color * depthfade;
}
