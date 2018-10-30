#version 440 core
#include "transform.inc"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;
layout(location=4) in uvec4 rig_bone;
layout(location=5) in vec4 rig_weight;

layout(push_constant, std140, row_major) uniform SceneSet 
{
  mat4 shadowview;

} scene;

layout(set=2, binding=0, std430, row_major) readonly buffer ModelSet 
{ 
  Transform modelworld;

  Transform bones[];

} model;

layout(location=4) out vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Transform modelworld = model.modelworld;
  
  Transform bone0 = model.bones[rig_bone[0]];
  Transform bone1 = model.bones[rig_bone[1]];
  Transform bone2 = model.bones[rig_bone[2]];
  Transform bone3 = model.bones[rig_bone[3]];
  
  Transform morph = transform_blend(rig_weight, bone0, bone1, bone2, bone3); 
  
  Transform morphworld = transform_multiply(modelworld, morph);
  
  texcoord = vertex_texcoord;
  
  gl_Position = map_parabolic(scene.shadowview * vec4(transform_multiply(morphworld, vertex_position), 1));
}
