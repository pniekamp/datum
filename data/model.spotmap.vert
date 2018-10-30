#version 440 core
#include "transform.inc"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

layout(push_constant, std140, row_major) uniform SceneSet 
{
  mat4 shadowview;

} scene;

layout(set=2, binding=0, std430, row_major) readonly buffer ModelSet 
{ 
  Transform modelworld;

} model;

layout(location=4) out vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Transform modelworld = model.modelworld;
  
  texcoord = vertex_texcoord;
  
  gl_Position = map_parabolic(scene.shadowview * vec4(transform_multiply(modelworld, vertex_position), 1));
}
