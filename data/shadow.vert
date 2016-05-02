#version 450 core
#include "transform.glsl"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

layout(std140, set=0, binding=0) uniform SceneSet 
{
  layout(row_major) mat4 proj;
  layout(row_major) mat4 invproj;
  layout(row_major) mat4 view;
  layout(row_major) mat4 invview;
  layout(row_major) mat4 worldview;
  vec3 camerapos;
  
} scene;

layout(std430, set=3, binding=0) buffer ModelSet 
{ 
  Transform modelworld;

} model;


layout(location=0) out vec2 texcoords;

///////////////////////// main //////////////////////////////////////////////
void main(void)
{
  texcoords = vertex_texcoord;
  
  gl_Position = vec4(transform_multiply(model.modelworld, vertex_position), 1.0);
}
