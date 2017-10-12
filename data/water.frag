#version 440 core
#include "bound.glsl"
#include "camera.glsl"
#include "gbuffer.glsl"
#include "transform.glsl"
#include "lighting.glsl"

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
  
  MainLight mainlight;

  uint environmentcount;
  Environment environments[MaxEnvironments];

  uint pointlightcount;
  PointLight pointlights[MaxPointLights];

  uint spotlightcount;
  SpotLight spotlights[MaxSpotLights];

  uint probecount;
  Probe probes[MaxProbes];
  
  uint decalcount;
  Decal decals[MaxDecals];

  Cluster cluster[];

} scene;

layout(set=0, binding=1) uniform sampler2D colormap;
layout(set=0, binding=11) uniform sampler2DArrayShadow shadowmap;
layout(set=0, binding=12) uniform sampler2DArray envbrdfmap;
layout(set=0, binding=13) uniform samplerCube envmaps[MaxEnvironments];
layout(set=0, binding=14) uniform sampler2DShadow spotmaps[MaxSpotLights];
layout(set=0, binding=15) uniform sampler2DArray decalmaps[MaxDecalMaps];

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;
  vec3 bumpscale;
  vec2 flow;

  Environment specular;

} params;

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform samplerCube specularmap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

layout(set=0, binding=9, input_attachment_index=3) uniform subpassInput depthmap;

layout(location=0) in vec3 position;
layout(location=1) in vec2 texcoord;
layout(location=2) in mat3 tbnworld;

layout(location=0) out vec4 fragcolor;

///////////////////////// mainlight_shadow //////////////////////////////////
float mainlight_shadow(MainLight light, vec3 position, vec3 normal, sampler2DArrayShadow shadowmap)
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
      return shadow_intensity(shadowmap, texel, spread[i]);
    }
  }
  
  return 1.0;
}

///////////////////////// spotlight_shadow //////////////////////////////////
float spotlight_shadow(SpotLight spotlight, vec3 position, vec3 normal, sampler2DShadow spotmap)
{
  vec3 shadowpos = position + 0.01 * normal;;
  vec4 shadowspace = map_parabolic(vec4(transform_multiply(spotlight.shadowview, shadowpos), 1));

  vec3 texel = vec3(0.5 * shadowspace.xy + 0.5, shadowspace.z);

  return shadow_intensity(spotmap, texel, 1.0);
}

///////////////////////// main //////////////////////////////////////////////
void main()
{
  ivec2 xy = ivec2(gl_FragCoord.xy);
  ivec2 viewport = textureSize(colormap, 0).xy;

  uint tile = cluster_tile(xy, viewport);
  uint tilez = cluster_tilez(gl_FragCoord.z);
  
  float bumpscale = params.bumpscale.z;

  vec4 bump0 = texture(normalmap, vec3(params.bumpscale.xy*(texcoord + params.flow), 0));
  vec4 bump1 = texture(normalmap, vec3(params.bumpscale.xy*(2.0*texcoord + 4.0*params.flow), 0));
  vec4 bump2 = texture(normalmap, vec3(params.bumpscale.xy*(4.0*texcoord + 8.0*params.flow), 0));

  vec3 normal = normalize(tbnworld * vec3((2*bump0.xy-1)*bump0.a + (2*bump1.xy-1)*bump1.a + (2*bump2.xy-1)*bump2.a, bumpscale)); 
  vec3 eyevec = normalize(scene.camera.position - position);

  float dist = view_depth(scene.proj, subpassLoad(depthmap).r) - view_depth(scene.proj, gl_FragCoord.z);

  float scale = 0.05 * dist;
  float facing = 1 - dot(eyevec, normal);

  vec4 color = textureLod(albedomap, vec3(clamp(vec2(scale, facing), 1/255.0, 254/255.0), 0), 0);
  
  Material material = make_material(color.rgb*params.color.rgb, params.emissive, params.metalness, params.reflectivity, params.roughness);
  
  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);

  MainLight mainlight = scene.mainlight;

  //
  // Environment Lighting
  //

  vec3 envdiffuse = vec3(0.2);
  vec3 envspecular = vec3(0);

  float ambientintensity = 1.0;
  
  vec3 diffusedirection = normal;
  vec3 speculardirection = reflect(-eyevec, normal);

  vec3 localpos = transform_multiply(params.specular.invtransform, position);
  vec3 localdiffuse = quaternion_multiply(params.specular.invtransform.real, diffusedirection);
  vec3 localspecular = quaternion_multiply(params.specular.invtransform.real, speculardirection);

  vec2 hittest = intersections(localpos, localspecular, params.specular.halfdim);

  vec3 localray = localpos + hittest.y * localspecular;
  
  envdiffuse = textureLod(specularmap, localdiffuse * vec3(1, -1, -1), 6.3).rgb;
  envspecular = textureLod(specularmap, localray * vec3(1, -1, -1), material.roughness * 8.0).rgb;

  vec3 envbrdf = texture(envbrdfmap, vec3(dot(normal, eyevec), material.roughness, 0)).xyz;

  env_light(diffuse, specular, material, envdiffuse, envspecular, envbrdf, ambientintensity);

  //
  // Main Light
  //

  float mainlightshadow = mainlight_shadow(mainlight, position, normal, shadowmap);
  
  if (mainlightshadow != 0)
  {      
    main_light(diffuse, specular, mainlight, normal, eyevec, material, mainlightshadow);
  }

  //
  // Point Lights
  //

  for(uint jm = scene.cluster[tile].pointlightmask[tilez], j = findLSB(jm); jm != 0; jm ^= (1 << j), j = findLSB(jm))
  {
    for(uint im = scene.cluster[tile].pointlightmasks[tilez][j], i = findLSB(im); im != 0; im ^= (1 << i), i = findLSB(im))
    {
      PointLight pointlight = scene.pointlights[(j << 5) + i];
      
      point_light(diffuse, specular, pointlight, position, normal, eyevec, material);
    }
  }

  //
  // Spot Lights
  //

  for(uint jm = scene.cluster[tile].spotlightmask[tilez], j = findLSB(jm); jm != 0; jm ^= (1 << j), j = findLSB(jm))
  {
    for(uint im = scene.cluster[tile].spotlightmasks[tilez][j], i = findLSB(im); im != 0; im ^= (1 << i), i = findLSB(im))
    {
      SpotLight spotlight = scene.spotlights[(j << 5) + i];
      
      float spotlightshadow = spotlight_shadow(spotlight, position, normal, spotmaps[(j << 5) + i]);
      
      if (spotlightshadow != 0)
      {
        spot_light(diffuse, specular, spotlight, position, normal, eyevec, material, spotlightshadow);
      }
    }
  }

  float fogfactor = clamp(exp2(-pow(params.color.a * dist, 2)), 0, 1); 

  fragcolor = vec4(scene.camera.exposure * ((diffuse + material.emissive) * material.diffuse + specular), mix(1, 1 - 0.8*envbrdf.x, fogfactor));
}
