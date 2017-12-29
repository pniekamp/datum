#version 440 core
#include "transform.glsl"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

layout(set=0, binding=0, std430, row_major) readonly buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;

} scene;

layout(set=2, binding=0, std430, row_major) readonly buffer ModelSet 
{ 
  Transform modelworld;
  vec2 uvscale;
  uint layers;

} model;

layout(location=0) out vec3 position;
layout(location=1) out mat3 tbnworld;
layout(location=4) out vec2 texcoord;
layout(location=5) out vec2 tilecoord;
layout(location=6) flat out uint layers;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Transform modelworld = model.modelworld;
  
  position = transform_multiply(modelworld, vertex_position);

  vec3 normal = quaternion_multiply(modelworld.real, vertex_normal);
  vec3 tangent = quaternion_multiply(modelworld.real, vertex_tangent.xyz);
  vec3 bitangent = cross(normal, tangent) * vertex_tangent.w;

  tbnworld = mat3(tangent, bitangent, normal);

  texcoord = vertex_texcoord;
  tilecoord = model.uvscale * vertex_texcoord;
  layers = model.layers;
  
  gl_Position = scene.worldview * vec4(position, 1);
}
