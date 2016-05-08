#version 450 core
#include "transform.glsl"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

layout(std430, set=0, binding=0) buffer SceneSet 
{
  layout(row_major) mat4 worldview;

} scene;

layout(std430, set=2, binding=0) buffer ModelSet 
{ 
  Transform modelworld;

} model;

layout(location=0) out vec2 texcoord;
layout(location=1) out mat3 tbnworld;

///////////////////////// main //////////////////////////////////////////////
void main(void)
{
  vec3 normal = quaternion_multiply(model.modelworld.real, vertex_normal);
  vec3 tangent = quaternion_multiply(model.modelworld.real, vertex_tangent.xyz);
  vec3 bitangent = cross(normal, tangent) * vertex_tangent.w;

  tbnworld = mat3(tangent, bitangent, normal);

  texcoord = vertex_texcoord;
  
  gl_Position = scene.worldview * vec4(transform_multiply(model.modelworld, vertex_position), 1.0);
}
