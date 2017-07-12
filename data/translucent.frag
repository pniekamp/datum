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

  Cluster cluster[];

} scene;

layout(set=0, binding=1) uniform sampler2D colormap;
layout(set=3, binding=4) uniform sampler2DArrayShadow shadowmap;
layout(set=3, binding=5) uniform sampler2DArray envbrdfmap;
layout(set=3, binding=6) uniform samplerCube envmaps[MaxEnvironments];
layout(set=3, binding=7) uniform sampler2DShadow spotmaps[MaxSpotLights];

layout(set=1, binding=0, std430, row_major) readonly buffer MaterialSet 
{
  vec4 color;
  float roughness;
  float reflectivity;
  float emissive;

} params;

layout(set=1, binding=1) uniform sampler2DArray albedomap;
layout(set=1, binding=2) uniform sampler2DArray surfacemap;
layout(set=1, binding=3) uniform sampler2DArray normalmap;

layout(location=0) in vec3 position;
layout(location=1) in mat3 tbnworld;
layout(location=4) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;

///////////////////////// mainlight_shadow //////////////////////////////////
float mainlight_shadow(MainLight mainlight, vec3 position, vec3 normal, sampler2DArrayShadow shadowmap)
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

  vec4 albedo = texture(albedomap, vec3(texcoord, 0));
  vec4 surface = texture(surfacemap, vec3(texcoord, 0));

  Material material = make_material(albedo.rgb * params.color.rgb, params.emissive, 0, params.reflectivity * surface.g, params.roughness * surface.a);

  vec3 normal = normalize(tbnworld * (2 * texture(normalmap, vec3(texcoord, 0)).xyz - 1));
  vec3 eyevec = normalize(scene.camera.position - position);

  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);

  MainLight mainlight = scene.mainlight;

  //
  // Environment Lighting
  //

  vec3 envdiffuse = vec3(0.2);
  vec3 envspecular = vec3(0);

  float ambientintensity = 1.0;
  
  vec3 diffusedirection = dffuse_dominantdirection(normal, eyevec, material.roughness);
  vec3 speculardirection = specular_dominantdirection(normal, reflect(-eyevec, normal), material.roughness);
  
  for(uint im = scene.cluster[tile].environmentmask[tilez], i = findLSB(im); im != 0; im ^= (1 << i), i = findLSB(im))
  {
    Environment environment = scene.environments[i];

    vec3 localpos = transform_multiply(environment.invtransform, position);
    vec3 localdiffuse = quaternion_multiply(environment.invtransform.real, diffusedirection);
    vec3 localspecular = quaternion_multiply(environment.invtransform.real, speculardirection);
    
    vec2 hittest = intersections(localpos, localspecular, environment.halfdim);
    
    if (hittest.y > max(hittest.x, 0) && hittest.x < 0)
    { 
      vec3 localray = localpos + hittest.y * localspecular;
      float localroughness = clamp(material.roughness * hittest.y / length(localray), 0, material.roughness);     

      envdiffuse = textureLod(envmaps[i], localdiffuse * vec3(1, -1, -1), 6.3).rgb;        
      envspecular = textureLod(envmaps[i], localray * vec3(1, -1, -1), localroughness * 8.0).rgb;
      
      break;
    }
  }

  vec2 envbrdf = texture(envbrdfmap, vec3(dot(normal, eyevec), material.roughness, 0)).rg;

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

  fragcolor = vec4(scene.camera.exposure * ((diffuse + material.emissive) * material.diffuse + specular), albedo.a * params.color.a);
}
