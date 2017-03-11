#version 440 core
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
  mat4 prevview;
  mat4 skyview;
  vec4 viewport;

} scene;

//layout(std140, push_constant, row_major) uniform ModelSet 
layout(std430, set=2, binding=0, row_major) buffer ModelSet 
{ 
  Transform modelworld;

} model;

layout(location=0) out vec3 position;
layout(location=1) out vec2 texcoord;
layout(location=2) out mat3 tbnworld;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  position = transform_multiply(model.modelworld, vertex_position);

  vec3 normal = quaternion_multiply(model.modelworld.real, vertex_normal);
  vec3 tangent = quaternion_multiply(model.modelworld.real, vertex_tangent.xyz);
  vec3 bitangent = cross(normal, tangent) * vertex_tangent.w;

  tbnworld = mat3(tangent, bitangent, normal);
  
  texcoord = vertex_texcoord;
  
  gl_Position = scene.worldview * vec4(position, 1);
}
