#version 440 core
#include "gbuffer.glsl"
#include "camera.glsl"

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

layout(set=0, binding=1) uniform sampler2D colormap;
layout(set=0, binding=6) uniform sampler2D depthmipmap;

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

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform sampler2DArray surfacemap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

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
  ivec2 xy = ivec2(gl_FragCoord.xy);
  ivec2 viewport = textureSize(colormap, 0).xy;

  float bumpscale = mix(1, 32, clamp(0.5*gl_FragCoord.w, 0, 1)) * params.bumpscale.z;

  vec4 bump0 = texture(normalmap, vec3(texcoord*params.bumpscale.xy + params.flow, 0));
  vec4 bump1 = texture(normalmap, vec3(texcoord*params.bumpscale.xy*2.0 + 4.0*params.flow, 0));
  vec4 bump2 = texture(normalmap, vec3(texcoord*params.bumpscale.xy*4.0 + 8.0*params.flow, 0));

  vec3 normal = normalize(tbnworld * vec3((2*bump0.xy-1)*bump0.a + (2*bump1.xy-1)*bump1.a + (2*bump2.xy-1)*bump2.a, bumpscale)); 
  vec3 eyevec = normalize(scene.camera.position - position);
  
  normal = normalize(normal - min(dot(normal, eyevec), 0) * eyevec);

  float dist = texelFetch(depthmipmap, xy/2, 0).g - view_depth(scene.proj, gl_FragCoord.z);

  float scale = 0.05 * dist;
  float facing = clamp(1 - dot(eyevec, tbnworld[2]), 0, 1);

  float roughness = mix(1, 0.4, FresnelBias + pow(facing, FresnelPower)) * params.roughness;
  float reflectivity = mix(1, 0.32, FresnelBias + pow(1 - facing, FresnelPower)) * params.reflectivity;

  vec4 albedo = textureLod(albedomap, vec3(clamp(dither(vec2(scale, facing)), 1/255.0, 254/255.0), 0), 0);

  float height = dot(params.foamplane.xyz, position) + params.foamplane.w; 

  vec3 wavefoam = texture(surfacemap, vec3(texcoord + 0.2*bump0.xy, 0)).rgb * clamp(pow(height - params.foamwaveheight, 3) * params.foamwavescale, 0, 1);
  
  vec3 shorefoam = (0.25 * texture(surfacemap, vec3(texcoord + 2.0*params.flow, 0)).rgb + 0.02) * clamp(height - (dist - params.foamshoreheight) * params.foamshorescale, 0, 1);

  fragcolor = vec4(albedo.rgb * params.color.rgb + wavefoam + shorefoam, params.emissive);
  fragspecular = vec4(vec3(reflectivity), roughness);
  fragnormal = vec4(0.5 * normal + 0.5, 0);
}
