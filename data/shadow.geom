#version 440 core
#include "camera.glsl"
#include "transform.glsl"
#include "lighting.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 12/*3*ShadowSlices*/) out;

layout(set=0, binding=0, std430, row_major) readonly buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;
  mat4 prevview;
  mat4 skyview;
  vec4 viewport;

  Camera camera;
  
  MainLight mainlight;

} scene;

layout(location=0) in vec2 texcoords[];
layout(location=0) out vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  for(int i = 0; i < ShadowSlices; ++i)
  {
    gl_Layer = i;

    gl_Position = scene.mainlight.shadowview[i] * gl_in[0].gl_Position;
    texcoord = texcoords[0];
    EmitVertex();

    gl_Position = scene.mainlight.shadowview[i] * gl_in[1].gl_Position;
    texcoord = texcoords[1];
    EmitVertex();

    gl_Position = scene.mainlight.shadowview[i] * gl_in[2].gl_Position;
    texcoord = texcoords[2];
    EmitVertex();
    
    EndPrimitive();
  }
}  
