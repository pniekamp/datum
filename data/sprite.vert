#version 450 core

layout (location = 0) in vec3 vertex_position;

layout (set = 0, binding = 0) uniform SceneSet 
{
  layout(row_major) mat4 proj;
  layout(row_major) mat4 invproj;
  layout(row_major) mat4 view;
  layout(row_major) mat4 invview;
  
  layout(row_major) mat4 worldview;

} scene;

layout (set = 1, binding = 0) uniform ModelSet 
{ 
  layout(row_major) mat4 modelworld;

} model;



///////////////////////// main //////////////////////////////////////////////
void main(void)
{
  gl_Position = scene.proj * vec4(25*vertex_position, 1.0);
}
