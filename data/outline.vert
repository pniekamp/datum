#version 450 core
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

layout(location=0) out vec2 texcoord;
layout(location=1) out vec4 fbocoord;

///////////////////////// main //////////////////////////////////////////////
void main(void)
{
  float bias = 0.01 * pow(-(scene.view * vec4(transform_multiply(model.modelworld, vertex_position), 1)).z, 0.6);

  vec3 normal = quaternion_multiply(model.modelworld.real, vertex_normal);

  vec4 ndc = scene.worldview * vec4(transform_multiply(model.modelworld, vertex_position + bias*normal), 1);

  texcoord = vertex_texcoord; 

  fbocoord = vec4(0.5 * ndc.xy / ndc.w + 0.5, ndc.z / ndc.w, 1);
  
  gl_Position = vec4(ndc.x * scene.viewport.z / (scene.viewport.z + 2*scene.viewport.x), ndc.y * scene.viewport.w / (scene.viewport.w + 2*scene.viewport.y), ndc.z, ndc.w);
}
