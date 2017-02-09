#version 450 core
#include "camera.glsl"
#include "gbuffer.glsl"

layout(origin_upper_left) in vec4 gl_FragCoord;

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
  
  Camera camera;

} scene;

layout(std430, set=1, binding=0, row_major) buffer MaterialSet 
{
  vec4 plane;
  vec4 color;
  float density;
  float falloff;
  float constant;
  float startdistance;

} material;

layout(set=0, binding=4) uniform sampler2D depthmap;

layout(location=0) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  float depth = texture(depthmap, texcoord).z;
  
  vec3 position = world_position(scene.invview, scene.proj, scene.invproj, texcoord, depth);
  
  vec3 V = scene.camera.position - position;

  float FdotC = dot(material.plane, vec4(scene.camera.position, 1));
  float FdotP = dot(material.plane, vec4(position, 1));
  float FdotV = dot(material.plane.xyz, V);
  
  float k = FdotC <= 0 ? 1 : 0;
  float c1 = min(k*FdotP, 0) + k*FdotC;
  float c2 = FdotP <= 0 ? (1-k)*FdotP : k*FdotC;

  float dist = max(min(-0.5 * material.falloff * (c1 - c2 * FdotP / abs(FdotV)), 1) * length(V) - material.startdistance, 0);
  
  //float dist = max(min(k - FdotP / abs(FdotV), 1) * length(V) - material.startdistance, 0);

  //float dist = max(length(V) - material.startdistance, 0);

  float factor = clamp(exp2(-pow(material.density * dist + k*material.constant, 2)), 0, 1);

  fragcolor = material.color * (1 - factor);
}
