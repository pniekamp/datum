#version 440 core
#include "transform.inc"
#include "camera.inc"
#include "gbuffer.inc"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

layout(constant_id = 7) const float FogDepthRange = 50.0;
layout(constant_id = 8) const float FogDepthExponent = 3.0;

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

  Camera camera;

} scene;

layout(set=0, binding=28) uniform sampler3D lightingmap;

struct Particle
{
  vec4 position;
  mat2 transform;
  vec3 color;
  uint alphaemissive;
};

layout(set=2, binding=0, std430, row_major) readonly buffer ModelSet
{
  Particle particles[];

} model;

layout(location=0) out vec3 texcoord;
layout(location=1) out vec4 lighting;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Particle particle = model.particles[gl_InstanceIndex];

  mat4 modelworld = { vec4(scene.view[0][0], scene.view[1][0], scene.view[2][0], 0), vec4(scene.view[0][1], scene.view[1][1], scene.view[2][1], 0), vec4(scene.view[0][2], scene.view[1][2], scene.view[2][2], 0), vec4(particle.position.xyz, 1) };

  float alpha = (particle.alphaemissive & 0xFFFF) * (1.0/65535);
  float emissive = ((particle.alphaemissive >> 16) & 0xFFFF) * (1.0/65535);

  texcoord = vec3(vertex_texcoord.s, 1 - vertex_texcoord.t, floor(particle.position.w));
  
  vec4 position = modelworld * vec4(particle.transform * vertex_position.xy, 0, 1);

  vec3 lightingtexcoord = (scene.view * position).xyz;
  lightingtexcoord.x = 0.5 * lightingtexcoord.x / (scene.invproj[0][0] * -lightingtexcoord.z) + 0.5;
  lightingtexcoord.y = 0.5 * lightingtexcoord.y / (scene.invproj[1][1] * -lightingtexcoord.z) + 0.5;
  lightingtexcoord.z = pow(max(-lightingtexcoord.z / FogDepthRange, 0), 1.0 / FogDepthExponent);

  vec3 diffuse = texture(lightingmap, lightingtexcoord).rgb;

  lighting = vec4((diffuse + 128*emissive*emissive*emissive) * particle.color, 1) * alpha;
  
  gl_Position = scene.worldview * position;
}
