#version 440 core
#include "transform.glsl"
#include "camera.glsl"
#include "lighting.glsl"

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
  
  Camera camera;
  
  MainLight mainlight;

  uint environmentcount;
  Environment environments[MaxEnvironments];

  uint pointlightcount;
  PointLight pointlights[MaxPointLights];

  uint spotlightcount;
  SpotLight spotlights[MaxSpotLights];

} scene;

layout(set=0, binding=5) uniform sampler2D colormap;
layout(set=0, binding=19) uniform samplerCube envmaps[MaxEnvironments];
layout(set=0, binding=22) uniform sampler3D fogmap;

layout(location=0) in vec3 texcoord;

layout(location=0) out vec4 fragcolor;

void main()
{
  vec3 skycolor = textureLod(envmaps[scene.environmentcount-1], texcoord, scene.camera.skyboxlod).rgb;
  
  vec4 fog = global_fog(vec3(gl_FragCoord.xy / textureSize(colormap, 0).xy, 1.0), fogmap);

  fragcolor = vec4(scene.camera.exposure * (skycolor * fog.a + fog.rgb), 0);
}
