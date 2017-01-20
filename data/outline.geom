#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

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

layout(location=0) in vec2 texcoords[];

layout(location=0) noperspective out vec4 fbocoord;
layout(location=1) out vec2 texcoord;

void EmitPt(vec4 pt, vec2 uv)
{
  texcoord = uv;
  fbocoord = vec4(0.5 * pt.xy/pt.w + 0.5, pt.z/pt.w, 1);
  gl_Position = vec4(pt.x * scene.viewport.z / (scene.viewport.z + 2*scene.viewport.x), pt.y * scene.viewport.w / (scene.viewport.w + 2*scene.viewport.y), pt.z, pt.w);
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
