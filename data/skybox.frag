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

layout(set=0, binding=1) uniform samplerCube skyboxmap;

layout(location=0) in vec3 texcoord;

layout(location=0) out vec4 fragcolor;

void main()
{
  fragcolor = vec4(scene.camera.exposure * textureLod(skyboxmap, texcoord, scene.camera.skyboxlod).rgb, 0);
}
