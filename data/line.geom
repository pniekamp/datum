#version 440 core
#include "transform.inc"

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

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
  vec3 size;
  float halfwidth;
  float overhang;

} model;

layout(location=0) noperspective out float offset;

void EmitPt(vec4 position, float dist)
{
  offset = dist;
  gl_Position = position;
  EmitVertex();
}

void EmitEdge(vec4 p0, vec4 p1)
{
  vec2 v = normalize(p1.xy/p1.w - p0.xy/p0.w);
  vec2 e = vec2(v.x / scene.viewport.z, v.y / scene.viewport.w) * model.overhang;
  vec2 n = vec2(-v.y / scene.viewport.z, v.x / scene.viewport.w) * model.halfwidth;
  float d = model.halfwidth / (model.halfwidth - 2);
  
  EmitPt(vec4(p0.xy + (n - e)*p0.w, p0.z, p0.w), d); 
  EmitPt(vec4(p1.xy + (n + e)*p1.w, p1.z, p1.w), d); 
  EmitPt(vec4(p0.xy - (n + e)*p0.w, p0.z, p0.w), -d); 
  EmitPt(vec4(p1.xy - (n - e)*p1.w, p1.z, p1.w), -d); 
  EndPrimitive();
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  EmitEdge(gl_in[0].gl_Position, gl_in[1].gl_Position);
}  
