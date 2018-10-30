#version 440 core
#include "transform.inc"
#include "camera.inc"

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

layout(set=0, binding=1) uniform sampler repeatsampler;
layout(set=0, binding=2) uniform sampler clampedsampler;

layout(set=0, binding=9) uniform sampler2D depthmap;

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;
  float depthfade;
  
} params;

layout(set=1, binding=1) uniform texture2DArray albedomap;
layout(set=1, binding=2) uniform texture2DArray surfacemap;
layout(set=1, binding=3) uniform texture2DArray normalmap;

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
  
  if (texture(depthmap, fbocoord).r > gl_FragCoord.z)
  {
    depthfade = params.depthfade;
 
    if (depthfade == 0.0)
      discard;
  }

  vec3 eyevec = normalize(scene.camera.position - position);

  vec4 albedo = texture(sampler2DArray(albedomap, repeatsampler), vec3(texcoord, 0));
  vec4 surface = texture(sampler2DArray(surfacemap, repeatsampler), vec3(texcoord, 0));
  
  vec3 lightintensity = vec3(0.5, 0.445, 0.485);

  vec3 diffuse = (0.2 + 0.8*max(dot(normal, eyevec), 0)) * lightintensity;
  vec3 specular = pow(max(dot(0.5*(eyevec + eyevec), normal), 0), 240 - 180*params.roughness) * max(1.0 - diffuse, 0.0) * lightintensity;

  fragcolor = vec4((diffuse * (albedo.rgb * params.color.rgb) + specular) * depthfade, 1) * albedo.a * params.color.a;
}
