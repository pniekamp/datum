#version 450 core
#include "camera.glsl"
#include "transform.glsl"
#include "lighting.glsl"

/*layout(constant_id = 46)*/ const uint ShadowSlices = 4;

layout(triangles) in;
layout(triangle_strip, max_vertices = 3*ShadowSlices) out;

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

  Camera camera;
  
  MainLight mainlight;

  float splits[4];
  mat4 shadowview[4];

} scene;

layout(location=0) in vec2 texcoords[];
layout(location=0) out vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  for(int i = 0; i < ShadowSlices; ++i)
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
