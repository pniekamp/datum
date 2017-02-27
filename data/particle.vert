#version 450 core

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

layout(std430, set=0, binding=0, row_major) buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;
  
} scene;

struct Particle
{
  vec4 position;
  mat2 transform;
  vec4 color;
};

layout(std430, set=2, binding=0, row_major) buffer ModelSet
{
  Particle particles[];

} model;

layout(location=0) out vec3 texcoord;
layout(location=1) flat out vec4 tint;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Particle particle = model.particles[gl_InstanceIndex];

  texcoord = vec3(vertex_texcoord.s, 1 - vertex_texcoord.t, particle.position.w);

  tint = particle.color;
  
  mat4 modelworld = { vec4(scene.view[0][0], scene.view[1][0], scene.view[2][0], 0), vec4(scene.view[0][1], scene.view[1][1], scene.view[2][1], 0), vec4(scene.view[0][2], scene.view[1][2], scene.view[2][2], 0), vec4(particle.position.xyz, 1) };
  
  gl_Position = scene.worldview * modelworld * vec4(particle.transform * vertex_position.xy, 0, 1);
}
