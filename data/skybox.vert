#version 440 core

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
 
} scene;

layout(location=0) out vec3 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  texcoord = (scene.skyview * vec4(vertex_position.xy, -1, 1)).xyz * vec3(1, -1, -1);

  gl_Position = vec4(vertex_position.xy, 0, 1);
}
