#version 450 core
#include "transform.glsl"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

layout(std430, set=0, binding=0, row_major) buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;

} scene;

//layout(std140, push_constant, row_major) uniform ModelSet 
layout(std430, set=2, binding=0, row_major) buffer ModelSet 
{ 
  Transform modelworld;

} model;

layout(location=1) out vec3 normal;

///////////////////////// main //////////////////////////////////////////////
void main(void)
{
  normal = quaternion_multiply(model.modelworld.real, vertex_normal);
  
  gl_Position = scene.worldview * vec4(transform_multiply(model.modelworld, vertex_position), 1);
}