#version 440 core
#include "camera.glsl"
#include "gbuffer.glsl"

layout(constant_id = 26) const bool DepthOfField = false;

layout(set=0, binding=0, std430, row_major) readonly buffer SceneSet 
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

layout(set=0, binding=1) uniform sampler2D colormap;
layout(set=0, binding=2) uniform sampler2D diffusemap;
layout(set=0, binding=3) uniform sampler2D specularmap;
layout(set=0, binding=4) uniform sampler2D normalmap;
layout(set=0, binding=5) uniform sampler2D depthmap;
layout(set=0, binding=6) uniform sampler2D depthmipmap;
layout(set=0, binding=12) uniform sampler2D ssaomap;
layout(set=0, binding=21) uniform sampler2D bloommap;
layout(set=0, binding=23) uniform sampler2D ssrmap;

layout(location=0) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  float dof = 0;
  
  if (DepthOfField)
  {
    dof = smoothstep(0, scene.camera.focalwidth, abs(scene.camera.focaldistance - view_depth(scene.proj, texture(depthmap, texcoord).r)));
  }

  vec3 color = textureLod(colormap, texcoord, dof).rgb;
  
  vec3 ssr = scene.camera.ssrstrength * texture(ssrmap, texcoord).rgb * (1 - dof);
  vec3 bloom = scene.camera.bloomstrength * texture(bloommap, texcoord).rgb;

//  fragcolor = texture(colormap, texcoord);
//  fragcolor = texture(diffusemap, texcoord);
//  fragcolor = texture(specularmap, texcoord);
//  fragcolor = texture(normalmap, texcoord);
//  fragcolor = texture(depthmap, texcoord);
//  fragcolor = textureLod(depthmipmap, texcoord, 5);
//  fragcolor = texture(ssaomap, texcoord).rrrr;
//  fragcolor = texture(ssrmap, texcoord);
//  fragcolor = texture(bloommap, texcoord);
  fragcolor = vec4(tonemap(color.rgb + ssr) + bloom, 1);
}
