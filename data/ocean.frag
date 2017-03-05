#version 450 core
#include "gbuffer.glsl"
#include "camera.glsl"

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
  vec4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;

  float bumpscale;

  vec4 foamplane;
  float foamwaveheight;
  float foamwavescale;
  float foamshoreheight;
  float foamshorescale;

  vec2 flow;
  
} material;

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform sampler2DArray specularmap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

layout(set=0, binding=4) uniform sampler2D depthmap;
layout(set=0, binding=6) uniform sampler2DArrayShadow shadowmap;

layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in mat3 tbnworld;

layout(location=0) out vec4 fragrt0;
layout(location=1) out vec4 fragrt1;
layout(location=2) out vec4 fragnormal;

vec2 dither(vec2 uv)
{
  return uv + fract(vec2(dot(vec2(171.0, 231.0), gl_FragCoord.st)) / vec2(103.0, 71.0)) / 512.0;
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  float depth = texelFetch(depthmap, ivec2(gl_FragCoord.xy), 0).r;

  if (depth < gl_FragCoord.z)
    discard;

  vec4 bump0 = texture(normalmap, vec3(texcoord + material.flow, 0));
  vec4 bump1 = texture(normalmap, vec3(2.0*texcoord + 4.0*material.flow, 0));
  vec4 bump2 = texture(normalmap, vec3(4.0*texcoord + 8.0*material.flow, 0));

  vec3 normal = normalize(tbnworld * (vec3(0, 0, 1) + material.bumpscale * ((2*bump0.rgb-1)*bump0.a + (2*bump1.rgb-1)*bump1.a + (2*bump2.rgb-1)*bump2.a))); 

  float dist = view_depth(scene.proj, depth) - view_depth(scene.proj, gl_FragCoord.z);

  float scale = 0.05 * dist;
  float facing = 1 - dot(normalize(scene.camera.position - position), normal);

  vec4 color = material.color * textureLod(albedomap, vec3(clamp(dither(vec2(scale, facing)), 1/255.0, 254/255.0), 0), 0);

  float height = dot(material.foamplane.xyz, position) + material.foamplane.w; 

  vec3 wavefoam = texture(albedomap, vec3(texcoord + 0.2*bump0.xy, 1)).rgb * clamp(pow(height - material.foamwaveheight, 3) * material.foamwavescale, 0, 1);
  
  vec3 shorefoam = (0.25 * texture(albedomap, vec3(texcoord + 8.0*material.flow, 1)).rgb + 0.02) * clamp(height - (dist - material.foamshoreheight) * material.foamshorescale, 0, 1);
  
  fragrt0 = vec4(color.rgb + wavefoam + shorefoam, material.emissive);
  fragrt1 = vec4(material.metalness, material.reflectivity, 0, material.roughness);
  fragnormal = vec4(0.5 * normal + 0.5, 1);
}
