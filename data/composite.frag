#version 440 core
#include "camera.inc"
#include "gbuffer.inc"

layout(constant_id = 26) const bool DepthOfField = false;
layout(constant_id = 27) const bool ColorGrading = false;

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
  vec4 fbosize;
  vec4 viewport;

  Camera camera;

} scene;

layout(set=0, binding=5) uniform sampler2D colormap;
layout(set=0, binding=6) uniform sampler2D diffusemap;
layout(set=0, binding=7) uniform sampler2D specularmap;
layout(set=0, binding=8) uniform sampler2D normalmap;
layout(set=0, binding=9) uniform sampler2D depthmap;
layout(set=0, binding=10) uniform sampler2D depthmipmap;
layout(set=0, binding=16) uniform sampler2D ssaomap;
layout(set=0, binding=31) uniform sampler2D bloommap;
layout(set=0, binding=33) uniform sampler2D ssrmap;
layout(set=0, binding=39) uniform sampler3D colorlut;

layout(location=0) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  float dof = 0;
  
  if (DepthOfField)
  {
    float depth = texture(depthmap, texcoord).r;
    
    dof = smoothstep(0, scene.camera.focalwidth, abs(scene.camera.focaldistance - view_depth(scene.proj, depth)));
  }

  vec3 color = textureLod(colormap, texcoord, 0.5*dof).rgb;
  
  vec3 ssr = scene.camera.ssrstrength * texture(ssrmap, texcoord).rgb * (1 - dof);
  vec3 bloom = scene.camera.bloomstrength * texture(bloommap, texcoord).rgb;

  if (ColorGrading)
  {
    fragcolor = vec4(colorgrad(colorlut, tonemap(color.rgb + ssr) + bloom), 1);
  }
  else
  {
    fragcolor = vec4(tonemap(color.rgb + ssr) + bloom, 1);
  }

//  fragcolor = texture(colormap, texcoord);
//  fragcolor = textureLod(colormap, texcoord, 1);
//  fragcolor = texture(diffusemap, texcoord);
//  fragcolor = texture(specularmap, texcoord);
//  fragcolor = texture(normalmap, texcoord);
//  fragcolor = texture(depthmap, texcoord);
//  fragcolor = textureLod(depthmipmap, texcoord, 5);
//  fragcolor = texture(ssaomap, texcoord).rrrr;
//  fragcolor = texture(ssrmap, texcoord);
//  fragcolor = texture(bloommap, texcoord);
}
