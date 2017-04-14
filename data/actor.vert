#version 440 core
#include "transform.glsl"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;
layout(location=4) in uvec4 rig_bone;
layout(location=5) in vec4 rig_weight;

layout(std430, set=0, binding=0, row_major) buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;

} scene;

layout(std430, set=2, binding=0, row_major) buffer ModelSet 
{ 
  Transform modelworld;

  Transform bones[];

} model;

layout(location=0) out vec2 texcoord;
layout(location=1) out mat3 tbnworld;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Transform morph = transform_blend(rig_weight, model.bones[rig_bone[0]], model.bones[rig_bone[1]], model.bones[rig_bone[2]], model.bones[rig_bone[3]]); 
  
  Transform modelworld = transform_multiply(model.modelworld, morph);
  
  vec3 normal = quaternion_multiply(modelworld.real, vertex_normal);
  vec3 tangent = quaternion_multiply(modelworld.real, vertex_tangent.xyz);
  vec3 bitangent = cross(normal, tangent) * vertex_tangent.w;

  tbnworld = mat3(tangent, bitangent, normal);

  texcoord = vertex_texcoord;
  
  gl_Position = scene.worldview * vec4(transform_multiply(modelworld, vertex_position), 1);
}
