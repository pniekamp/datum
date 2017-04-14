#version 440 core
#include "transform.glsl"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;
layout(location=4) in uvec4 rig_bone;
layout(location=5) in vec4 rig_weight;

layout(std430, set=2, binding=0, row_major) buffer ModelSet 
{ 
  Transform modelworld;

  Transform bones[];

} model;

layout(location=0) out vec2 texcoords;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Transform morph = transform_blend(rig_weight, model.bones[rig_bone[0]], model.bones[rig_bone[1]], model.bones[rig_bone[2]], model.bones[rig_bone[3]]); 
  
  Transform modelworld = transform_multiply(model.modelworld, morph);
  
  texcoords = vertex_texcoord;
  
  gl_Position = vec4(transform_multiply(modelworld, vertex_position), 1);
}
