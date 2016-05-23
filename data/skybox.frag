#version 450 core
#include "camera.glsl"

layout(std430, set=0, binding=0) buffer SceneSet 
{
  layout(row_major) mat4 proj;
  layout(row_major) mat4 invproj;
  layout(row_major) mat4 view;
  layout(row_major) mat4 invview;
  layout(row_major) mat4 prevview;
  layout(row_major) mat4 skyview;
  
  Camera camera;

} scene;

layout(set=0, binding=1) uniform samplerCube skyboxmap;

layout(location=0) in vec3 texcoord;

layout(location=0) out vec4 fragcolor;

void main()
{
  fragcolor = vec4(tonemap(scene.camera.exposure * texture(skyboxmap, texcoord).rgb), 1);
}
