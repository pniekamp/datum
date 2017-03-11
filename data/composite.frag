#version 440 core
#include "camera.glsl"

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
  
  Camera camera;

} scene;

layout(set=0, binding=1) uniform sampler2D rt0map;
layout(set=0, binding=2) uniform sampler2D rt1map;
layout(set=0, binding=3) uniform sampler2D normalmap;
layout(set=0, binding=4) uniform sampler2D depthmap;
layout(set=0, binding=5) uniform sampler2D colormap;
layout(set=0, binding=6) uniform sampler2D ssrmap;
layout(set=0, binding=7) uniform sampler2D bloommap;

layout(location=0) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec3 color = texture(colormap, texcoord).rgb;
  
  vec3 ssr = scene.camera.ssrstrength * texture(ssrmap, texcoord).rgb;
  vec3 bloom = scene.camera.bloomstrength * texture(bloommap, texcoord).rgb;
  
  fragcolor = vec4(tonemap(color.rgb + ssr) + bloom, 1);
//  fragcolor = vec4(texture(colormap, texcoord).rgb, 1);
//  fragcolor = vec4(texture(rt0map, texcoord).rgb, 1);
//  fragcolor = vec4(texture(rt1map, texcoord).rgb, 1);
//  fragcolor = vec4(texture(colormap, texcoord).rgb, 1);
//  fragcolor = vec4(texture(normalmap, texcoord).rgb, 1);
}
