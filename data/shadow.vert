#version 450 core
#include "transform.glsl"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

//layout(std140, push_constant, row_major) uniform ModelSet 
layout(std430, set=2, binding=0, row_major) buffer ModelSet 
{ 
  Transform modelworld;

} model;

layout(location=0) out vec2 texcoords;

///////////////////////// main //////////////////////////////////////////////
void main(void)
{
  texcoords = vertex_texcoord;
  
  gl_Position = vec4(transform_multiply(model.modelworld, vertex_position), 1);
}
