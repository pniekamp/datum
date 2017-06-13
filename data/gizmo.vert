#version 440 core
#include "transform.glsl"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

layout(std430, set=0, binding=0, row_major) readonly buffer SceneSet 
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

layout(std430, set=2, binding=0, row_major) readonly buffer ModelSet 
{ 
  Transform modelworld;
  vec3 size;

} model;

layout(location=0) out vec3 position;
layout(location=1) out vec2 texcoord;
layout(location=2) out vec3 normal;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Transform modelworld = model.modelworld;

  position = transform_multiply(modelworld, model.size * vertex_position);
  normal = quaternion_multiply(modelworld.real, vertex_normal);

  texcoord = vertex_texcoord; 

  gl_Position = (scene.worldview * vec4(position, 1)) * vec4(scene.viewport.z / (scene.viewport.z + 2*scene.viewport.x), scene.viewport.w / (scene.viewport.w + 2*scene.viewport.y), 1, 1);  
}
