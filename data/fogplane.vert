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

} model;

layout(location=0) out vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  texcoord = vec2(vertex_texcoord.s, vertex_texcoord.t);

  gl_Position = vec4(vertex_position.xy, max(scene.proj[3][2] / (2 * model.modelworld.dual.w) - scene.proj[2][2], 0), 1);
}
