#version 450 core

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
  mat4 prevview;
  mat4 skyview;
  
} scene;

layout(location=0) out vec3 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main(void)
{
  vec2 pos = (2 * vertex_position.xy - 1);

  texcoord = (scene.skyview * vec4(pos, -1.0, 1.0)).xyz * vec3(1, -1, -1);

  gl_Position = vec4(pos, 1.0, 1.0);
}