#version 440 core
#include "transform.inc"

layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out;

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
  vec4 fbosize;
  vec4 viewport;
  
} scene;

layout(set=2, binding=0, std430, row_major) readonly buffer ModelSet 
{
  Transform modelworld;
  float halfwidth;
  float overhang;

} model;

void EmitPt(vec4 position)
{
  gl_Position = position;
  EmitVertex();
}

void EmitEdge(vec4 p0, vec4 p1)
{
  vec2 v = normalize(p1.xy/p1.w - p0.xy/p0.w);
  vec2 e = vec2(v.x / scene.viewport.z, v.y / scene.viewport.w) * model.overhang;
  vec2 n = vec2(-v.y / scene.viewport.z, v.x / scene.viewport.w) * model.halfwidth;

  EmitPt(vec4(p0.xy + (n - e)*p0.w, p0.z, p0.w)); 
  EmitPt(vec4(p1.xy + (n + e)*p1.w, p1.z, p1.w)); 
  EmitPt(vec4(p0.xy - (n + e)*p0.w, p0.z, p0.w)); 
  EmitPt(vec4(p1.xy - (n - e)*p1.w, p1.z, p1.w)); 
  EndPrimitive();
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec2 p0 = gl_in[0].gl_Position.xy/gl_in[0].gl_Position.w;
  vec2 p1 = gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w;
  vec2 p2 = gl_in[2].gl_Position.xy/gl_in[2].gl_Position.w;
  
  float area = (p2.x - p0.x) * (p1.y - p0.y) - (p2.y - p0.y) * (p1.x - p0.x);

  if (area > 0)
  {
    EmitEdge(gl_in[0].gl_Position, gl_in[1].gl_Position);
    EmitEdge(gl_in[1].gl_Position, gl_in[2].gl_Position);
    EmitEdge(gl_in[2].gl_Position, gl_in[0].gl_Position);
  }
}  
