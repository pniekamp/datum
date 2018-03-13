#version 440 core
#include "camera.glsl"
#include "gbuffer.glsl"

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

} scene;

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 plane;
  vec4 color;
  float density;
  float falloff;
  float startdistance;

} params;

layout(set=0, binding=11, input_attachment_index=0) uniform subpassInput depthmap;

layout(location=0) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  float depth = subpassLoad(depthmap).r;
  
  vec3 position = world_position(scene.invview, scene.proj, scene.invproj, texcoord, depth);
  
  vec3 V = scene.camera.position - position;

  float FdotC = dot(params.plane, vec4(scene.camera.position, 1));
  float FdotP = dot(params.plane, vec4(position, 1));
  float FdotV = dot(params.plane.xyz, V);
  
  float k = FdotC <= 0 ? 1 : 0;
  float c1 = min(k*FdotP, 0) + k*FdotC;
  float c2 = FdotP <= 0 ? (1-k)*FdotP : k*FdotC;

  float dist = max(min(-0.5 * params.falloff * (c1 - c2 * FdotP / abs(FdotV)), 1) * length(V) - params.startdistance, 0);
  
  //float dist = max(min(k - FdotP / abs(FdotV), 1) * length(V) - params.startdistance, 0);

  //float dist = max(length(V) - params.startdistance, 0);

  float factor = clamp(exp2(-pow(params.density * dist, 2)), 0, 1);

  fragcolor = vec4(scene.camera.exposure * params.color.rgb, 1) * params.color.a * (1 - factor);
}
