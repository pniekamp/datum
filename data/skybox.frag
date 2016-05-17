#version 450 core
#include "camera.glsl"

layout(std430, set=0, binding=0) buffer SceneSet 
{
  layout(row_major) mat4 modelview;
  
  float exposure;
  
} scene;

layout(set=0, binding=1) uniform samplerCube skyboxmap;

layout(location=0) in vec3 texcoord;

layout(location=0) out vec4 fragcolor;

void main()
{
  fragcolor = vec4(tonemap(scene.exposure * texture(skyboxmap, texcoord).rgb), 1);
}
