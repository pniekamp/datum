#version 440 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

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
  
} scene;

layout(location=0) noperspective out vec3 edgedist;

void EmitPt(vec4 position, vec3 dist)
{
  edgedist = dist; 
  gl_Position = position;
  EmitVertex();
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec2 p0 = scene.viewport.z * gl_in[0].gl_Position.xy/gl_in[0].gl_Position.w;
  vec2 p1 = scene.viewport.z * gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w;
  vec2 p2 = scene.viewport.z * gl_in[2].gl_Position.xy/gl_in[2].gl_Position.w;
  
  float area = abs((p2.x - p0.x) * (p1.y - p0.y) - (p2.y - p0.y) * (p1.x - p0.x));

  EmitPt(gl_in[0].gl_Position, vec3(area/length(p2 - p1), 0.0, 0.0));
  EmitPt(gl_in[1].gl_Position, vec3(0.0, area/length(p2 - p0), 0.0));
  EmitPt(gl_in[2].gl_Position, vec3(0.0, 0.0, area/length(p1 - p0)));  
  EndPrimitive();
}  
