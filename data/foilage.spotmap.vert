#version 440 core
#include "transform.glsl"

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
  vec4 wind;
  vec3 bendscale;
  vec3 detailbendscale;
  Transform modelworlds[];

} model;

layout(location=0) out vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Transform modelworld = model.modelworlds[gl_InstanceIndex];

  vec3 localposition = vertex_position;
  
  vec3 localwind = quaternion_multiply(quaternion_conjugate(modelworld.real), model.wind.xyz);
  
  localposition = transform_detailbend(localposition, modelworld.dual.yzw, model.wind.w, localwind, model.detailbendscale);;
  localposition = transform_bend(localposition, localwind, model.bendscale);

  texcoord = vertex_texcoord;
  
  gl_Position = map_parabolic(scene.shadowview * vec4(transform_multiply(modelworld, localposition), 1));
}
