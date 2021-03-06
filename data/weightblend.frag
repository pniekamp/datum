#version 440 core
#include "transform.inc"
#include "camera.inc"
#include "lighting.inc"

layout(set=0, binding=0, std430, row_major) readonly buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;
  mat4 orthoview;

} scene;

layout(set=0, binding=33) uniform sampler2D colormap;
layout(set=0, binding=34) uniform sampler2D weightmap;

layout(location=0) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;

void main()
{
  vec4 color = texelFetch(colormap, ivec2(gl_FragCoord.xy), 0);
  
  if (color.a == 1)
    discard;

  float weight = texelFetch(weightmap, ivec2(gl_FragCoord.xy), 0).r;
  
  fragcolor = vec4(color.rgb / clamp(weight, 1e-9, 5e9), 1 - color.a);
}
