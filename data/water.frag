#version 440 core
#include "bound.glsl"
#include "camera.glsl"
#include "gbuffer.glsl"
#include "transform.glsl"
#include "lighting.glsl"

layout(constant_id = 46) const uint ShadowSlices = 4;
layout(constant_id = 29) const uint MaxPointLights = 256;
layout(constant_id = 31) const uint MaxEnvironments = 6;

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
  
  MainLight mainlight;

  float splits[4];
  mat4 shadowview[4];
  
  uint environmentcount;
  Environment environments[MaxEnvironments];

  uint pointlightcount;
  PointLight pointlights[MaxPointLights];

} scene;

layout(std430, set=1, binding=0, row_major) buffer MaterialSet 
{
  vec4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;
  float bumpscale;
  vec2 flow;

  Environment specular;

} material;

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform samplerCube specularmap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

layout(set=0, binding=4) uniform sampler2D depthmap;
layout(set=0, binding=6) uniform sampler2DArrayShadow shadowmap;
layout(set=0, binding=7) uniform sampler2DArray envbrdfmap;

layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in mat3 tbnworld;

layout(location=0) out vec4 fragcolor;

///////////////////////// mainlight_shadow //////////////////////////////////
float mainlight_shadow(MainLight light, vec3 position, vec3 normal)
{
  const float bias[ShadowSlices] = { 0.05, 0.06, 0.10, 0.25 };
  const float spread[ShadowSlices] = { 1.5, 1.2, 1.0, 0.2 };

  for(uint i = 0; i < ShadowSlices; ++i)
  {
    vec3 shadowpos = position + bias[i] * normal;
    vec4 shadowspace = scene.shadowview[i] * vec4(shadowpos, 1);
    
    vec4 texel = vec4(0.5 * shadowspace.xy + 0.5, i, shadowspace.z);

    if (texel.x > 0.0 && texel.x < 1.0 && texel.y > 0.0 && texel.y < 1.0 && texel.w > 0.0 && texel.w < 1.0)
    { 
      return shadow_intensity(scene.shadowview[i], shadowpos, i, shadowmap, spread[i]);
    }
  }
  
  return 1.0;
}

///////////////////////// environment  ///////////////////////////////////////
void environment(inout vec3 envdiffuse, inout vec3 envspecular, vec3 position, vec3 normal, vec3 eyevec)
{
  vec3 diffusedirection = normal;
  vec3 speculardirection = reflect(-eyevec, normal);

  vec3 localpos = transform_multiply(material.specular.invtransform, position);
  vec3 localdiffuse = quaternion_multiply(material.specular.invtransform.real, diffusedirection);
  vec3 localspecular = quaternion_multiply(material.specular.invtransform.real, speculardirection);

  vec2 hittest = intersections(localpos, localspecular, material.specular.halfdim);

  vec3 localray = localpos + hittest.y * localspecular;
  
  envdiffuse = textureLod(specularmap, localdiffuse * vec3(1, -1, -1), 6.3).rgb;
  envspecular = textureLod(specularmap, localray * vec3(1, -1, -1), material.roughness * 8.0).rgb;
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  float depth = texelFetch(depthmap, ivec2(gl_FragCoord.xy), 0).r;
  
  vec4 bump0 = texture(normalmap, vec3(texcoord + material.flow, 0));
  vec4 bump1 = texture(normalmap, vec3(2.0*texcoord + 4.0*material.flow, 0));
  vec4 bump2 = texture(normalmap, vec3(4.0*texcoord + 8.0*material.flow, 0));

  float bumpscale = material.bumpscale * mix(1, 0.2, max(500*(gl_FragCoord.z - 0.998), 0));

  vec3 normal = normalize(tbnworld * (vec3(0, 0, 1) + bumpscale * ((2*bump0.rgb-1)*bump0.a + (2*bump1.rgb-1)*bump1.a + (2*bump2.rgb-1)*bump2.a))); 

  vec3 eyevec = normalize(scene.camera.position - position);

  float dist = view_depth(scene.proj, depth) - view_depth(scene.proj, gl_FragCoord.z);

  float scale = 0.05 * dist;
  float facing = 1 - dot(eyevec, normal);

  vec4 color = textureLod(albedomap, vec3(clamp(vec2(scale, facing), 1/255.0, 254/255.0), 0), 0);
  
  Material surface = make_material(color.rgb*material.color.rgb, material.emissive, material.metalness, material.reflectivity, material.roughness);

  vec2 envbrdf = texture(envbrdfmap, vec3(dot(normal, eyevec), surface.roughness, 0)).rg;

  float ambientintensity = 1.0;

  float mainlightshadow = mainlight_shadow(scene.mainlight, position, normal);

  float fogfactor = clamp(exp2(-pow(material.color.a * dist, 2)), 0, 1); 
  
  vec3 envdiffuse = vec3(0.2);
  vec3 envspecular = vec3(0);

  environment(envdiffuse, envspecular, position, normal, eyevec);

  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);

  env_light(diffuse, specular, surface, envdiffuse, envspecular, envbrdf, ambientintensity);

  main_light(diffuse, specular, scene.mainlight, normal, eyevec, surface, mainlightshadow);

  fragcolor = vec4(scene.camera.exposure * ((diffuse + surface.emissive) * surface.diffuse + specular), mix(1, 1 - 0.8*envbrdf.x, fogfactor));
}
