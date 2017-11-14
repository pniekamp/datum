
const uint MaxShadowSplits = 4;
const uint MaxEnvironments = 8;
const uint MaxPointLights = 512;
const uint MaxSpotLights = 16;
const uint MaxProbes = 128;
const uint MaxDecals = 128;
const uint MaxDecalMaps = 16;
const uint ClusterSizeZ = 24;

layout(constant_id = 4) const uint ClusterTileX = 64;
layout(constant_id = 5) const uint ClusterTileY = 64;
layout(constant_id = 6) const uint ShadowSlices = 4;
layout(constant_id = 7) const float FogDepthRange = 50.0;
layout(constant_id = 8) const float FogDepthExponent = 3.0;

//----------------------- Main Light ----------------------------------------
//---------------------------------------------------------------------------

struct MainLight
{
  vec3 direction;
  vec3 intensity;
  
  float splits[MaxShadowSplits];
  mat4 shadowview[MaxShadowSplits];
};

//----------------------- Point Light ---------------------------------------
//---------------------------------------------------------------------------

struct PointLight
{
  vec3 position;
  vec3 intensity;
  vec4 attenuation;
};

//----------------------- Spot Light ----------------------------------------
//---------------------------------------------------------------------------

struct SpotLight
{
  vec3 position;
  vec3 intensity;
  vec4 attenuation;
  vec3 direction;
  float cutoff;
  Transform shadowview;
};


//----------------------- Probe ---------------------------------------------
//---------------------------------------------------------------------------

struct Probe
{
  vec4 position;
  float irradiance[9][3];
};


//----------------------- Environment ---------------------------------------
//---------------------------------------------------------------------------

struct Environment
{
  vec3 halfdim;
  Transform invtransform;
};


//----------------------- Decal ---------------------------------------------
//---------------------------------------------------------------------------

struct Decal
{
  vec3 halfdim;
  Transform invtransform;
  vec4 color;
  float metalness;
  float roughness;
  float reflectivity;
  float emissive;
  vec4 texcoords;
  float layer;
  uint albedomap;
  uint normalmap;
  uint mask;
};


//----------------------- Cluster -------------------------------------------
//---------------------------------------------------------------------------

struct Cluster
{
  uint environmentmask[ClusterSizeZ];
  
  uint pointlightmask[ClusterSizeZ];
  uint pointlightmasks[ClusterSizeZ][(MaxPointLights + 31)/32];

  uint spotlightmask[ClusterSizeZ];
  uint spotlightmasks[ClusterSizeZ][(MaxSpotLights + 31)/32];

  uint probemask[ClusterSizeZ];
  uint probemasks[ClusterSizeZ][(MaxDecals + 31)/32];

  uint decalmask[ClusterSizeZ];
  uint decalmasks[ClusterSizeZ][(MaxDecals + 31)/32];
};

uint cluster_tile(vec2 uv, ivec2 viewport)
{
  uint ClusterSizeX = uint(float(viewport.x)/ClusterTileX + float(ClusterTileX - 1)/ClusterTileX);
  uint ClusterSizeY = uint(float(viewport.y)/ClusterTileY + float(ClusterTileY - 1)/ClusterTileY);

  return uint(uv.y*ClusterSizeY)*ClusterSizeX + uint(uv.x*ClusterSizeX);
}

uint cluster_tile(ivec2 xy, ivec2 viewport)
{
  return cluster_tile((vec2(xy) + 0.5)/viewport, viewport);
}

uint cluster_tilez(float depth)
{
  return clamp(uint(pow(depth, 64) * ClusterSizeZ), 0, ClusterSizeZ-1);
}

float cluster_depth(uint tilez)
{
  return pow(tilez * (1.0/ClusterSizeZ), 1.0/64.0);
}

//----------------------- Material ------------------------------------------
//---------------------------------------------------------------------------

struct Material
{
  vec3 diffuse;
  vec3 specular;
  float emissive;
  float roughness;
  float alpha;
};


//----------------------- Poisson Disk --------------------------------------
//---------------------------------------------------------------------------

const vec2 PoissonDisk[12] = {
  vec2(-0.1711046, -0.425016),
  vec2(-0.7829809, 0.2162201),
  vec2(-0.2380269, -0.8835521),
  vec2(0.4198045, 0.1687819),
  vec2(-0.684418, -0.3186957),
  vec2(0.6026866, -0.2587841),
  vec2(-0.2412762, 0.3913516),
  vec2(0.4720655, -0.7664126),
  vec2(0.9571564, 0.2680693),
  vec2(-0.5238616, 0.802707),
  vec2(0.5653144, 0.60262),
  vec2(0.0123658, 0.8627419)
};


//----------------------- Functions -----------------------------------------
//---------------------------------------------------------------------------

#define PI 3.1415926535897932384626433832795

///////////////////////// make_material /////////////////////////////////////
Material make_material(vec3 albedo, float emissive, float metalness, float reflectivity, float roughness)
{
  Material material;

  material.emissive = 128*emissive*emissive*emissive;

  material.diffuse = albedo * (1 - metalness);
  material.specular = mix(vec3(0.16 * reflectivity * reflectivity), albedo, metalness);

  material.roughness = roughness;
  material.alpha = material.roughness * material.roughness;;

  return material;
}

///////////////////////// make_material /////////////////////////////////////
Material make_material(vec4 diffusemap, vec4 specularmap)
{
  Material material;

  material.emissive = 128*diffusemap.a*diffusemap.a*diffusemap.a;

  material.diffuse = diffusemap.rgb;
  material.specular = specularmap.rgb;

  material.roughness = specularmap.a;
  material.alpha = material.roughness * material.roughness;

  return material;
}

///////////////////////// mix_material //////////////////////////////////////
Material mix_material(Material first, Material second, float factor)
{ 
  Material material;

  material.emissive = mix(first.emissive, second.emissive, factor);
  material.diffuse = mix(first.diffuse, second.diffuse, factor);
  material.specular = mix(first.specular, second.specular, factor);
  material.roughness = mix(first.roughness, second.roughness, factor);
  material.alpha = material.roughness * material.roughness;

  return material;
}

///////////////////////// ambient_intensity /////////////////////////////////
float ambient_intensity(MainLight light, sampler2D ssaomap, ivec2 xy, ivec2 viewport)
{
  return texture(ssaomap, (vec2(xy) + 0.5)/viewport).x;
}

///////////////////////// shadow_split //////////////////////////////////////
vec4 shadow_split(float splits[4], uint nslices, float depth)
{
  vec4 a = vec4(smoothstep(0.75*vec3(splits[0], splits[1], splits[2]), vec3(splits[0], splits[1], splits[2]), vec3(depth)), 0);
  vec4 b = vec4(1, smoothstep(0.75*vec3(splits[0], splits[1], splits[2]), vec3(splits[0], splits[1], splits[2]), vec3(depth)));

  return mix((1-a)*b, vec4(0), lessThan(vec4(nslices), vec4(1, 2, 3, 4)));
}

///////////////////////// shadow_intensity //////////////////////////////////
float shadow_intensity(sampler2D shadowmap, ivec2 xy, ivec2 viewport)
{
  return texture(shadowmap, vec2(xy+0.5)/viewport).r;
}

///////////////////////// shadow_intensity //////////////////////////////////
float shadow_intensity(sampler2DShadow shadowmap, vec3 texel, float spread)
{
  float sum = 0;

  vec2 texelsize = spread / textureSize(shadowmap, 0).xy;

  for(uint k = 0; k < 12; ++k)
  {
    sum += texture(shadowmap, vec3(texel.xy + PoissonDisk[k]*texelsize, texel.z));
  }

  return sum * (1.0/12.0);
}

///////////////////////// shadow_intensity //////////////////////////////////
float shadow_intensity(sampler2DArrayShadow shadowmap, vec4 texel, float spread)
{
  float sum = 0;

  vec2 texelsize = spread / textureSize(shadowmap, 0).xy;

  for(uint k = 0; k < 12; ++k)
  {
    sum += texture(shadowmap, vec4(texel.xy + PoissonDisk[k]*texelsize, texel.z, texel.w));
  }

  return sum * (1.0/12.0);
}

///////////////////////// diffuse_intensity /////////////////////////////////
float diffuse_intensity(vec3 N, vec3 L)
{
  return max(dot(N, L), 0);
}

///////////////////////// specular_intensity ////////////////////////////////
float specular_intensity(vec3 N, vec3 V, vec3 L)
{
  //return max(dot(reflect(-L, N), V), 0);
  return max(dot(normalize(L + V), N), 0);
}

///////////////////////// specular_dominantdirection ////////////////////////
vec3 specular_dominantdirection(vec3 N, vec3 R, float roughness)
{
  float smoothness = 1 - roughness;

  return mix(N, R, smoothness * (sqrt(smoothness) + roughness));
}

///////////////////////// dffuse_dominantdirection //////////////////////////
vec3 dffuse_dominantdirection(vec3 N, vec3 V, float roughness)
{
  float a = 1.02341f * roughness - 1.51174f;
  float b = -0.511705f * roughness + 0.755868f;

  return mix(N, V, clamp((dot(N, V) * a + b) * roughness, 0, 1));
}

///////////////////////// fresnel_schlick ///////////////////////////////////
vec3 fresnel_schlick(vec3 f0, float f90, float u)
{
  return f0 + (f90 - f0) * pow(1 - u, 5.0);
}

///////////////////////// visibility_smith //////////////////////////////////
float visibility_smith(float NdotV, float NdotL, float alpha)
{
  float k = alpha / 2;
  float GGXL = NdotL * (1-k) + k;
  float GGXV = NdotV * (1-k) + k;

  return 0.25 / (GGXV * GGXL + 1e-5);
}

///////////////////////// distribution_ggx //////////////////////////////////
float distribution_ggx(float NdotH, float alpha)
{
  float alpha2 = alpha * alpha;
  float f = (NdotH * alpha2 - NdotH) * NdotH + 1;

  return alpha2 / (f * f);
}

/////////////////////////  diffuse_disney ///////////////////////////////////
float diffuse_disney(float NdotV, float NdotL, float LdotH, float alpha)
{
  float energybias = mix(0.0, 0.5, alpha);
  float energyfactor = mix(1.0, 1.0 / 1.51, alpha);
  float f90 = energybias + 2.0 * LdotH*LdotH * alpha;

  float lightscatter = fresnel_schlick(vec3(1), f90, NdotL).r;
  float viewscatter = fresnel_schlick(vec3(1), f90, NdotV).r;

  return lightscatter * viewscatter * energyfactor;
}

///////////////////////// specular_ggx ////////////////////////////////////
vec3 specular_ggx(vec3 f0, float f90, float NdotV, float NdotL, float LdotH, float NdotH, float alpha)
{
  vec3 Fc = fresnel_schlick(f0, f90, LdotH);
  float Vis = visibility_smith(NdotV, NdotL, alpha);
  float D = distribution_ggx(NdotH, alpha);

  return D * Vis * Fc;
}


///////////////////////// probe_irradiance //////////////////////////////////
float probe_irradiance(inout vec3 irradiance, Probe probe, vec3 position, vec3 normal)
{
  float L0 = 3.141593 * 0.282095;

  float L1 = 2.094395 * 0.488603 * normal.y;
  float L2 = 2.094395 * 0.488603 * normal.z;
  float L3 = 2.094395 * 0.488603 * normal.x;

  float L4 = 0.785398 * 1.092548 * normal.x * normal.y;
  float L5 = 0.785398 * 1.092548 * normal.y * normal.z;
  float L6 = 0.785398 * 0.315392 * (3 * normal.z*normal.z - 1);
  float L7 = 0.785398 * 1.092548 * normal.z * normal.x;
  float L8 = 0.785398 * 0.546274 * (normal.x*normal.x - normal.y*normal.y);

  float r = L0 * probe.irradiance[0][0] + L1 * probe.irradiance[1][0] + L2 * probe.irradiance[2][0] + L3 * probe.irradiance[3][0] + L4 * probe.irradiance[4][0] + L5 * probe.irradiance[5][0] + L6 * probe.irradiance[6][0] + L7 * probe.irradiance[7][0] + L8 * probe.irradiance[8][0];
  float g = L0 * probe.irradiance[0][1] + L1 * probe.irradiance[1][1] + L2 * probe.irradiance[2][1] + L3 * probe.irradiance[3][1] + L4 * probe.irradiance[4][1] + L5 * probe.irradiance[5][1] + L6 * probe.irradiance[6][1] + L7 * probe.irradiance[7][1] + L8 * probe.irradiance[8][1];
  float b = L0 * probe.irradiance[0][2] + L1 * probe.irradiance[1][2] + L2 * probe.irradiance[2][2] + L3 * probe.irradiance[3][2] + L4 * probe.irradiance[4][2] + L5 * probe.irradiance[5][2] + L6 * probe.irradiance[6][2] + L7 * probe.irradiance[7][2] + L8 * probe.irradiance[8][2];

  float probedistance = length(probe.position.xyz - position);

  float attenuation = pow(clamp(1 - pow(probedistance/probe.position.w, 4.0), 0, 1), 2);

  irradiance += max(vec3(r, g, b), 0) * attenuation;
  
  return attenuation;
}


///////////////////////// env_light /////////////////////////////////////////
void env_light(inout vec3 diffuse, inout vec3 specular, Material material, vec3 envdiffuse, vec3 envspecular, vec3 envbrdf, float ambientintensity)
{
  float f90 = 0.8f;

  diffuse += envdiffuse * envbrdf.z * ambientintensity;
  specular += envspecular * (material.specular * envbrdf.x + f90 * envbrdf.y) * ambientintensity;
}


///////////////////////// main_light ////////////////////////////////////////
void main_light(inout vec3 diffuse, inout vec3 specular, MainLight light, vec3 normal, vec3 eyevec, Material material, float shadowfactor)
{
  vec3 lightvec = -light.direction;

  vec3 halfvec = normalize(lightvec + eyevec);

  float NdotV = max(dot(normal, eyevec), 0);
  float NdotL = max(dot(normal, lightvec), 0);
  float NdotH = max(dot(normal, halfvec), 0);
  float LdotH = clamp(dot(lightvec, halfvec), 0, 1);

  float Fd = diffuse_disney(NdotV, NdotL, LdotH, material.alpha) * (1/PI);

  vec3 Fr = specular_ggx(material.specular, 1, NdotV, NdotL, LdotH, NdotH, material.alpha) * (1/PI);

  diffuse += NdotL * Fd * light.intensity * shadowfactor;

  specular += NdotL * Fr * light.intensity * shadowfactor;
}


///////////////////////// point_light ///////////////////////////////////////
void point_light(inout vec3 diffuse, inout vec3 specular, PointLight light, vec3 position, vec3 normal, vec3 eyevec, Material material)
{
  vec3 lightvec = normalize(light.position - position);

  vec3 halfvec = normalize(lightvec + eyevec);

  float NdotV = max(dot(normal, eyevec), 0);
  float NdotL = max(dot(normal, lightvec), 0);
  float NdotH = max(dot(normal, halfvec), 0);
  float LdotH = clamp(dot(lightvec, halfvec), 0, 1);

  float Fd = diffuse_disney(NdotV, NdotL, LdotH, material.alpha) * (1/PI);

  vec3 Fr = specular_ggx(material.specular, 1, NdotV, NdotL, LdotH, NdotH, material.alpha) * (1/PI);

  float lightdistance = length(light.position - position);

  float attenuation = sign(NdotL) / (light.attenuation.z + light.attenuation.y * lightdistance + light.attenuation.x * lightdistance * lightdistance);

  attenuation *= pow(clamp(1 - pow(lightdistance/light.attenuation.w, 4.0), 0, 1), 2);

  diffuse += NdotL * Fd * light.intensity * attenuation;

  specular += NdotL * Fr * light.intensity * attenuation;
}


///////////////////////// spot_light ////////////////////////////////////////
void spot_light(inout vec3 diffuse, inout vec3 specular, SpotLight light, vec3 position, vec3 normal, vec3 eyevec, Material material, float shadowfactor)
{
  vec3 lightvec = normalize(light.position - position);

  vec3 halfvec = normalize(lightvec + eyevec);

  float NdotV = max(dot(normal, eyevec), 0);
  float NdotL = max(dot(normal, lightvec), 0);
  float NdotH = max(dot(normal, halfvec), 0);
  float LdotH = max(dot(lightvec, halfvec), 0);

  float Fd = diffuse_disney(NdotV, NdotL, LdotH, material.alpha) * (1/PI);

  vec3 Fr = specular_ggx(material.specular, 1, NdotV, NdotL, LdotH, NdotH, material.alpha) * (1/PI);

  float lightdistance = length(light.position - position);

  float attenuation = sign(NdotL) / (light.attenuation.z + light.attenuation.y * lightdistance + light.attenuation.x * lightdistance * lightdistance);

  attenuation *= pow(clamp(1 - pow(lightdistance/light.attenuation.w, 4.0), 0, 1), 2);
  
  attenuation *= smoothstep(light.cutoff, light.cutoff+0.05, dot(light.direction, -lightvec));

  diffuse += NdotL * Fd * light.intensity * attenuation * shadowfactor;

  specular += NdotL * Fr * light.intensity * attenuation * shadowfactor;
}


///////////////////////// global_fog ////////////////////////////////////////
vec4 global_fog(vec3 xyz, sampler3D fogmap)
{
#if (defined(GLOBALFOG) && GLOBALFOG == 0)
  return vec4(0, 0, 0, 1);
#else
  return texture(fogmap, xyz);
#endif  
}

vec4 global_fog(vec2 texcoord, float viewdepth, sampler3D fogmap)
{
  return global_fog(vec3(texcoord, pow(viewdepth / FogDepthRange, 1.0f / FogDepthExponent)), fogmap);
}

vec4 global_fog(ivec2 xy, ivec2 viewport, float viewdepth, sampler3D fogmap)
{
  return global_fog((vec2(xy) + 0.5)/viewport, viewdepth, fogmap);
}
