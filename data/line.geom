#version 450 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

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
  
} scene;

layout(std430, set=1, binding=0, row_major) buffer MaterialSet 
{
  vec4 color;
  vec4 texcoords;
  float depthfade;
  float halfwidth;
  float overhang;

} material;

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
  vec2 e = vec2(v.x / scene.viewport.z, v.y / scene.viewport.w) * material.overhang;
  vec2 n = vec2(-v.y / scene.viewport.z, v.x / scene.viewport.w) * material.halfwidth;
  float d = material.halfwidth / (material.halfwidth - 2);
  
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