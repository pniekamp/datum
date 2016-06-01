#version 450 core
#include "transform.glsl"
#include "lighting.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3*NSLICES) out;

layout(std430, set=0, binding=0, row_major) buffer SceneSet 
{
  mat4 shadowview[NSLICES];

} scene;

layout(location=0) in vec2 texcoords[];
layout(location=0) out vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  for(int i = 0; i < NSLICES; ++i)
  {
    gl_Layer = i;

    gl_Position = scene.shadowview[i] * gl_in[0].gl_Position;
    texcoord = texcoords[0];
    EmitVertex();

    gl_Position = scene.shadowview[i] * gl_in[1].gl_Position;
    texcoord = texcoords[1];
    EmitVertex();

    gl_Position = scene.shadowview[i] * gl_in[2].gl_Position;
    texcoord = texcoords[2];
    EmitVertex();
    
    EndPrimitive();
  }
}  
