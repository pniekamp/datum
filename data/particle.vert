#version 440 core
#include "gbuffer.glsl"
#include "camera.glsl"
#include "transform.glsl"
#include "lighting.glsl"

layout(location=0) in vec3 vertex_position;
layout(location=1) in vec2 vertex_texcoord;
layout(location=2) in vec3 vertex_normal;
layout(location=3) in vec4 vertex_tangent;

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

  uint environmentcount;
  Environment environments[MaxEnvironments];

  uint pointlightcount;
  PointLight pointlights[MaxPointLights];

  uint spotlightcount;
  SpotLight spotlights[MaxSpotLights];

  uint probecount;
  Probe probes[MaxProbes]; 

  uint decalcount;
  Decal decals[MaxDecals];

  Cluster cluster[];

} scene;

layout(set=0, binding=13) uniform sampler2DArrayShadow shadowmap;

struct Particle
{
  vec4 position;
  mat2 transform;
  vec4 color;
};

layout(set=2, binding=0, std430, row_major) readonly buffer ModelSet
{
  Particle particles[];

} model;

layout(location=0) out vec3 texcoord;
layout(location=1) out vec4 tint;

///////////////////////// mainlight_shadow //////////////////////////////////
float mainlight_shadow(MainLight mainlight, vec3 position)
{
  vec4 shadowspace = scene.mainlight.shadowview[2] * vec4(position, 1);

  return texture(shadowmap, vec4(0.5 * shadowspace.xy + 0.5, 2, shadowspace.z));
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Particle particle = model.particles[gl_InstanceIndex];

  mat4 modelworld = { vec4(scene.view[0][0], scene.view[1][0], scene.view[2][0], 0), vec4(scene.view[0][1], scene.view[1][1], scene.view[2][1], 0), vec4(scene.view[0][2], scene.view[1][2], scene.view[2][2], 0), vec4(particle.position.xyz, 1) };
  
  vec4 position = modelworld * vec4(particle.transform * vertex_position.xy, 0, 1);
  vec3 normal = scene.view[1].xyz;
  vec3 eyevec = normalize(scene.camera.position - position.xyz);

  vec3 diffuse = vec3(0.5);
  
  MainLight mainlight = scene.mainlight;

  float mainlightshadow = mainlight_shadow(mainlight, position.xyz/position.w);
  
  diffuse += 0.5 * diffuse_intensity(normal, -mainlight.direction) * mainlight.intensity * mainlightshadow;
  
  texcoord = vec3(vertex_texcoord.s, 1 - vertex_texcoord.t, particle.position.w);

  tint = vec4(diffuse * particle.color.rgb, 1) * particle.color.a;

  gl_Position = scene.worldview * position;
}
