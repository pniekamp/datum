#version 440 core
#include "bound.glsl"
#include "camera.glsl"
#include "gbuffer.glsl"
#include "transform.glsl"
#include "lighting.glsl"

layout(std430, set=0, binding=0, row_major) readonly buffer SceneSet 
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

} scene;

layout(std430, set=1, binding=0, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;
  vec3 bumpscale;
  vec2 flow;

  Environment specular;

} material;

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform samplerCube specularmap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

layout(set=0, binding=4) uniform sampler2D depthmap;
layout(set=0, binding=5) uniform sampler2DArrayShadow shadowmap;
layout(set=0, binding=6) uniform sampler2DArray envbrdfmap;

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
    vec4 shadowspace = scene.mainlight.shadowview[i] * vec4(shadowpos, 1);
    
    vec4 texel = vec4(0.5 * shadowspace.xy + 0.5, i, shadowspace.z);

    if (texel.x > 0.0 && texel.x < 1.0 && texel.y > 0.0 && texel.y < 1.0 && texel.w > 0.0 && texel.w < 1.0)
    { 
      return shadow_intensity(scene.mainlight.shadowview[i], shadowpos, i, shadowmap, spread[i]);
    }
  }
  
  return 1.0;
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  float floordepth = texelFetch(depthmap, ivec2(gl_FragCoord.xy), 0).r;
  
  float bumpscale = material.bumpscale.z;

  vec4 bump0 = texture(normalmap, vec3(material.bumpscale.xy*(texcoord + material.flow), 0));
  vec4 bump1 = texture(normalmap, vec3(material.bumpscale.xy*(2.0*texcoord + 4.0*material.flow), 0));
  vec4 bump2 = texture(normalmap, vec3(material.bumpscale.xy*(4.0*texcoord + 8.0*material.flow), 0));

  vec3 normal = normalize(tbnworld * vec3((2*bump0.xy-1)*bump0.a + (2*bump1.xy-1)*bump1.a + (2*bump2.xy-1)*bump2.a, bumpscale)); 

  vec3 eyevec = normalize(scene.camera.position - position);

  float dist = view_depth(scene.proj, floordepth) - view_depth(scene.proj, gl_FragCoord.z);

  float scale = 0.05 * dist;
  float facing = 1 - dot(eyevec, normal);

  vec4 color = textureLod(albedomap, vec3(clamp(vec2(scale, facing), 1/255.0, 254/255.0), 0), 0);
  
  Material surface = make_material(color.rgb*material.color.rgb, material.emissive, material.metalness, material.reflectivity, material.roughness);

  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);
  
  vec3 envdiffuse = vec3(0.2);
  vec3 envspecular = vec3(0);

  vec3 diffusedirection = normal;
  vec3 speculardirection = reflect(-eyevec, normal);

  vec3 localpos = transform_multiply(material.specular.invtransform, position);
  vec3 localdiffuse = quaternion_multiply(material.specular.invtransform.real, diffusedirection);
  vec3 localspecular = quaternion_multiply(material.specular.invtransform.real, speculardirection);

  vec2 hittest = intersections(localpos, localspecular, material.specular.halfdim);

  vec3 localray = localpos + hittest.y * localspecular;
  
  envdiffuse = textureLod(specularmap, localdiffuse * vec3(1, -1, -1), 6.3).rgb;
  envspecular = textureLod(specularmap, localray * vec3(1, -1, -1), material.roughness * 8.0).rgb;

  vec2 envbrdf = texture(envbrdfmap, vec3(dot(normal, eyevec), surface.roughness, 0)).rg;

  env_light(diffuse, specular, surface, envdiffuse, envspecular, envbrdf, 1);

  MainLight mainlight = scene.mainlight;
  
  float mainlightshadow = mainlight_shadow(mainlight, position, normal);

  main_light(diffuse, specular, mainlight, normal, eyevec, surface, mainlightshadow);

  float fogfactor = clamp(exp2(-pow(material.color.a * dist, 2)), 0, 1); 

  fragcolor = vec4(scene.camera.exposure * ((diffuse + surface.emissive) * surface.diffuse + specular), mix(1, 1 - 0.8*envbrdf.x, fogfactor));
}
