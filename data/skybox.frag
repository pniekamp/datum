#version 450 core
#include "camera.glsl"

layout(std430, set=0, binding=0, row_major) buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 prevview;
  mat4 skyview;
  
  Camera camera;

} scene;

layout(set=0, binding=1) uniform samplerCube skyboxmap;

layout(location=0) in vec3 texcoord;

layout(location=0) out vec4 fragcolor;

void main()
{
  fragcolor = vec4(tonemap(scene.camera.exposure * textureLod(skyboxmap, texcoord, 0).rgb), 1);
}