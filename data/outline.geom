#version 440 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

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

layout(location=0) in vec2 texcoords[];

layout(location=0) out vec2 texcoord;

void EmitPt(vec4 position, vec2 uv)
{
  texcoord = uv;
  gl_Position = position;
  EmitVertex();
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  EmitPt(gl_in[0].gl_Position, texcoords[0]);
  EmitPt(gl_in[1].gl_Position, texcoords[1]);
  EmitPt(gl_in[2].gl_Position, texcoords[2]);
  EndPrimitive();
}  
