#version 440 core
#include "transform.inc"
#include "camera.inc"

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
  mat4 orthoview;
  mat4 prevview;
  mat4 skyview;
  vec4 fbosize;
  vec4 viewport;

  Camera camera;
  
} scene;

layout(set=0, binding=1) uniform sampler repeatsampler;
layout(set=0, binding=2) uniform sampler clampedsampler;

layout(set=2, binding=0, std430, row_major) readonly buffer ModelSet 
{ 
  Transform modelworld;
  vec4 texcoords;
  float morphbeg;
  float morphend;
  float morphgrid;
  vec4 areascale;
  vec2 uvscale;
  uint layers;

} model;

layout(set=3, binding=0) uniform texture2DArray heightmap;
layout(set=3, binding=1) uniform texture2DArray normalmap;
layout(set=3, binding=2) uniform texture2DArray blendmap;

layout(location=4) out vec2 texcoord;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  Transform modelworld = model.modelworld;

  vec3 camerapos = transform_multiply(transform_inverse(modelworld), scene.camera.position);
  vec2 gridpos = floor(vertex_position.xy / model.morphgrid) * model.morphgrid;

  float alpha = smoothstep(model.morphbeg, model.morphend, distance(camerapos.xy, vertex_position.xy * model.areascale.xy));
  
  vec2 xy = mix(vertex_position.xy, gridpos, alpha);
  vec2 uv = vec2(model.texcoords.zw * xy + model.texcoords.xy); 

  float height = texture(sampler2DArray(heightmap, clampedsampler), vec3(uv, 0)).r;
  
  vec3 vertexpos = vec3(xy * model.areascale.xy, height);

  gl_Position = scene.worldview * vec4(transform_multiply(modelworld, vertexpos), 1);
}
