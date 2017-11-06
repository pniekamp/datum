#version 440 core
#include "camera.glsl"
#include "transform.glsl"
#include "lighting.glsl"

layout(set=0, binding=0, std430, row_major) readonly buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;

} scene;

layout(set=0, binding=23) uniform sampler2D colormap;
layout(set=0, binding=24) uniform sampler2D weightmap;

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
