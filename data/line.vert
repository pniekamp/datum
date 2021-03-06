#version 440 core
#include "transform.inc"

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
  mat4 orthoview;
  mat4 prevview;
  mat4 skyview;
  vec4 fbosize;
  vec4 viewport;
  
} scene;

layout(set=2, binding=0, std430, row_major) readonly buffer ModelSet 
{
  Transform modelworld;
  vec3 size;
  float halfwidth;
  float overhang;

} model;

///////////////////////// main //////////////////////////////////////////////
void main()
{ 
  Transform modelworld = model.modelworld;

  vec3 position = transform_multiply(modelworld, model.size * vertex_position);

  gl_Position = (scene.worldview * vec4(position, 1)) * vec4(scene.viewport.z / (scene.viewport.z + 2*scene.viewport.x), scene.viewport.w / (scene.viewport.w + 2*scene.viewport.y), 1, 1);  

}
