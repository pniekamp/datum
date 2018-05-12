#version 440 core
#include "camera.glsl"
#include "transform.glsl"
#include "lighting.glsl"

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
  vec4 viewport;

  Camera camera;

} scene;

layout(set=0, binding=5) uniform sampler2D depthmap;

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;
  float depthfade;
  
} params;

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform sampler2DArray surfacemap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in vec3 normal;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{ 
  vec2 fbocoord = (gl_FragCoord.xy - scene.viewport.xy) / scene.viewport.zw;

  if (fbocoord.x < 0 || fbocoord.x > 1 || fbocoord.y < 0 || fbocoord.y > 1)
    discard;

  float depthfade = 1.0;
  
  if (texture(depthmap, fbocoord).r < gl_FragCoord.z)
  {
    depthfade = params.depthfade;
 
    if (depthfade == 0.0)
      discard;
  }

  vec4 albedo = texture(albedomap, vec3(texcoord, 0));
  vec4 surface = texture(surfacemap, vec3(texcoord, 0));

  Material material = make_material(albedo.rgb * params.color.rgb, params.emissive, params.metalness * surface.r, params.reflectivity * surface.g, params.roughness * surface.a);
  
  vec3 eyevec = normalize(scene.camera.position - position);

  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);

  env_light(diffuse, specular, material, vec3(1), vec3(0), vec3(0), 0.2);

  MainLight mainlight;
  mainlight.direction = -eyevec;
  mainlight.intensity = vec3(1.0, 0.945, 0.985);

  main_light(diffuse, specular, mainlight, normal, eyevec, material, 1);

  fragcolor = vec4((diffuse * material.diffuse + specular) * depthfade, 1) * albedo.a * params.color.a;
}
