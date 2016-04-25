#version 450 core
#include "transform.glsl"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;

layout(std140, set=0, binding=0) uniform SceneSet 
{
  layout(row_major) mat4 worldview;

} scene;

layout(std430, set=2, binding=0) buffer ModelSet 
{ 
  Transform modelworld;

} model;

layout(location=0) out vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main(void)
{
  texcoord = vertex_texcoord;
  
  gl_Position = scene.worldview * vec4(transform_multiply(model.modelworld , vertex_position), 1.0);
}
