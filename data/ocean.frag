#version 440 core
#include "camera.inc"
#include "gbuffer.inc"

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

layout(set=0, binding=5) uniform sampler2D colormap;
layout(set=0, binding=34) uniform sampler2D depthsrcmap;

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;

  vec3 bumpscale;

  vec4 foamplane;
  float foamwaveheight;
  float foamwavescale;
  float foamshoreheight;
  float foamshorescale;

  vec2 flow;
  
} params;

layout(set=1, binding=1) uniform texture2DArray albedomap;
layout(set=1, binding=2) uniform texture2DArray surfacemap;
layout(set=1, binding=3) uniform texture2DArray normalmap;

layout(location=0) in vec3 position;
layout(location=1) in mat3 tbnworld;
layout(location=4) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;
layout(location=1) out vec4 fragspecular;
layout(location=2) out vec4 fragnormal;

const float FresnelBias = 0.328;
const float FresnelPower = 5.0;

vec2 dither(vec2 uv)
{
  return uv + fract(vec2(dot(vec2(171.0, 231.0), gl_FragCoord.st)) / vec2(103.0, 71.0)) / 512.0;
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 fbosize = scene.fbosize;
  ivec2 xy = ivec2(gl_FragCoord.xy);

  vec4 bump0 = texture(sampler2DArray(normalmap, repeatsampler), vec3(texcoord*params.bumpscale.xy + params.flow, 0));
  vec4 bump1 = texture(sampler2DArray(normalmap, repeatsampler), vec3(texcoord*params.bumpscale.xy*2.0 + 4.0*params.flow, 0));
  vec4 bump2 = texture(sampler2DArray(normalmap, repeatsampler), vec3(texcoord*params.bumpscale.xy*4.0 + 8.0*params.flow, 0));

  vec3 normal = normalize(tbnworld * vec3((2*bump0.xy-1)*bump0.a + (2*bump1.xy-1)*bump1.a + (2*bump2.xy-1)*bump2.a, params.bumpscale.z));
  vec3 eyevec = normalize(scene.camera.position - position);
  
  float dist = texelFetch(depthsrcmap, xy, 0).r - view_depth(scene.proj, gl_FragCoord.z);

  float scale = clamp(0.05 * dist, 1e-3, 1);
  float facing = clamp(1 - dot(eyevec, tbnworld[2]), 0, 1);
  
  vec4 albedo = texture(sampler2DArray(albedomap, clampedsampler), vec3(dither(vec2(scale, facing)), 0));

  float roughness = mix(0, params.roughness, clamp(FresnelBias + pow(facing, FresnelPower), 0, 1));
  
  normal = mix(tbnworld[2], normal, clamp(2 * dot(normal, eyevec), 0, 1));
  
  float height = dot(params.foamplane.xyz, position) + params.foamplane.w; 

  vec3 wavefoam = texture(sampler2DArray(surfacemap, repeatsampler), vec3(texcoord + 0.2*bump0.xy, 0)).rgb * clamp(pow(height - params.foamwaveheight, 3) * params.foamwavescale, 0, 1);
  
  vec3 shorefoam = (0.25 * texture(sampler2DArray(surfacemap, repeatsampler), vec3(texcoord + 2.0*params.flow, 0)).rgb + 0.02) * clamp(height - (dist - params.foamshoreheight) * params.foamshorescale, 0, 1);

  fragcolor = vec4(albedo.rgb * params.color.rgb + wavefoam + shorefoam, params.emissive);
  fragspecular = vec4(params.color.rgb * params.reflectivity, roughness);
  fragnormal = vec4(0.5 * normal + 0.5, 0);
}
