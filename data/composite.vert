#version 440 core

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

layout(location=0) out vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  texcoord = vertex_texcoord.xy;
  
  gl_Position = vec4(vertex_position.xy, 0, 1);
}
