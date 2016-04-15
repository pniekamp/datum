#version 450 core

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;

layout(std140, set=0, binding=0) uniform SceneSet 
{
  layout(row_major) mat4 proj;
  layout(row_major) mat4 invproj;
  layout(row_major) mat4 view;
  layout(row_major) mat4 invview;
  
  layout(row_major) mat4 worldview;

} scene;

layout(std140, set=1, binding=0) buffer MaterialSet 
{
  vec4 tint;
  vec4 texcoords;

} material;

layout(std430, set=2, binding=0) buffer ModelSet 
{ 
  vec2 xbasis;
  vec2 ybasis;
  vec4 position;

} model;

layout(location=0) out vec3 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main(void)
{
  mat4 modelworld = { vec4(model.xbasis, 0, 0), vec4(model.ybasis, 0, 0), vec4(0), vec4(model.position.xy, 0, 1) };

  texcoord = vec3(material.texcoords.xy + material.texcoords.zw * vertex_texcoord, model.position.z);
  
  gl_Position = scene.worldview * modelworld * vec4(vertex_position, 1.0);
}
