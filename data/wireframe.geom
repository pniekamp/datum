#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location=0) in vec4 fbocoords[];

layout(location=0) out vec4 fbocoord;
layout(location=1) noperspective out vec3 edgedist;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec2 p0 = 1000.0 * gl_in[0].gl_Position.xy/gl_in[0].gl_Position.w;
  vec2 p1 = 1000.0 * gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w;
  vec2 p2 = 1000.0 * gl_in[2].gl_Position.xy/gl_in[2].gl_Position.w;
  
  float area = abs((p2.x - p0.x) * (p1.y - p0.y) - (p2.y - p0.y) * (p1.x - p0.x));

  gl_Position = gl_in[0].gl_Position;
  fbocoord = fbocoords[0];
  edgedist = vec3(area/length(p2 - p1), 0.0, 0.0);
  EmitVertex();

  gl_Position = gl_in[1].gl_Position;
  fbocoord = fbocoords[1];
  edgedist = vec3(0.0, area/length(p2 - p0), 0.0);
  EmitVertex();

  gl_Position = gl_in[2].gl_Position;
  fbocoord = fbocoords[2];
  edgedist = vec3(0.0, 0.0, area/length(p1 - p0));
  EmitVertex();
  
  EndPrimitive();
}  
