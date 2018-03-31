#version 440 core

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;

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
  vec2 xbasis;
  vec2 ybasis;
  vec4 position;
  vec4 texcoords;
  float layers;

} model;

layout(push_constant, std140, row_major) uniform ParamSet
{ 
  mat4 orthoview;

} params;

layout(location = 0) out vec3 texcoord0;
layout(location = 1) out vec3 texcoord1;
layout(location = 2) out float texblend;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  mat4 modelworld = { vec4(model.xbasis, 0, 0), vec4(model.ybasis, 0, 0), vec4(0), vec4(model.position.xy, 0, 1) };

  texcoord0 = vec3(model.texcoords.xy + model.texcoords.zw * vertex_texcoord, floor(model.position.z));
  texcoord1 = vec3(model.texcoords.xy + model.texcoords.zw * vertex_texcoord, floor(model.position.w));
  
  texblend = fract(model.position.z);
  
  gl_Position = params.orthoview * modelworld * vec4(0.5 * vertex_position + 0.5, 1);
}
