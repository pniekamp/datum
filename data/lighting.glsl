
//----------------------- Main Light ----------------------------------------
//---------------------------------------------------------------------------

struct MainLight
{
  vec3 direction;
  vec3 intensity;
};

//----------------------- Point Light ---------------------------------------
//---------------------------------------------------------------------------

struct PointLight
{
  vec3 position;
  vec3 intensity;
  vec4 attenuation;
};


//----------------------- Environment ---------------------------------------
//---------------------------------------------------------------------------

struct Environment
{
  vec3 halfdim;
  Transform invtransform;
};


//----------------------- Material ------------------------------------------
//---------------------------------------------------------------------------

struct Material
{
  vec3 albedo;
  vec3 diffuse;
  vec3 specular;
  float emissive;
  float metalness;
  float roughness;
  float reflectivity;
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


///////////////////////// unpack_material ///////////////////////////////////
Material unpack_material(vec4 rt0, vec4 rt1)
{
  Material material;

  material.albedo = rt0.rgb;
  material.emissive = 128*rt0.a*rt0.a*rt0.a;
  material.metalness = rt1.r;
  material.roughness = rt1.a;
  material.reflectivity = rt1.g;

  material.diffuse = material.albedo * (1 - material.metalness);
  material.specular = mix(vec3(0.16 * material.reflectivity * material.reflectivity), material.albedo, material.metalness);

  material.alpha = material.roughness * material.roughness;

  return material;
}


///////////////////////// ambient_intensity /////////////////////////////////
float ambient_intensity(MainLight light, sampler2D ssaomap, ivec2 xy, ivec2 viewport)
{
  return texture(ssaomap, vec2(xy+0.5)/viewport).x;
}

///////////////////////// shadow_split //////////////////////////////////////
vec4 shadow_split(float splits[4], uint nslices, float depth)
{
  vec4 a = vec4(smoothstep(0.75*vec3(splits[0], splits[1], splits[2]), vec3(splits[0], splits[1], splits[2]), vec3(depth)), 0);
  vec4 b = vec4(1, smoothstep(0.75*vec3(splits[0], splits[1], splits[2]), vec3(splits[0], splits[1], splits[2]), vec3(depth)));

  return mix((1-a)*b, vec4(0), lessThan(vec4(nslices), vec4(1, 2, 3, 4)));
}

///////////////////////// shadow_intensity //////////////////////////////////
float shadow_intensity(mat4 shadowview, vec3 position, uint split, sampler2DArrayShadow shadowmap, float spread)
{
  vec4 shadowspace = shadowview * vec4(position, 1);

  vec4 texel = vec4(0.5 * shadowspace.xy + 0.5, split, shadowspace.z);

  vec2 texelsize = spread / textureSize(shadowmap, 0).xy;

  float sum = 0;

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
  vec3 F = fresnel_schlick(f0, f90, LdotH);
  float Vis = visibility_smith(NdotV, NdotL, alpha);
  float D = distribution_ggx(NdotH, alpha);

  return D * F * Vis;
}


///////////////////////// env_light  ////////////////////////////////////////
void env_light(inout vec3 diffuse, inout vec3 specular, Material material, vec3 envdiffuse, vec3 envspecular, vec2 envbrdf, float ambientintensity)
{
  float f90 = 0.8f;

  diffuse += envdiffuse * material.diffuse * ambientintensity;
  specular += envspecular * (material.specular * envbrdf.x + f90 * envbrdf.y) * ambientintensity;
}


///////////////////////// main_light  ///////////////////////////////////////
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


///////////////////////// point_light  //////////////////////////////////////
void point_light(inout vec3 diffuse, inout vec3 specular, PointLight light, vec3 position, vec3 normal, vec3 eyevec, Material material)
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

  diffuse += NdotL * Fd * light.intensity * attenuation;

  specular += NdotL * Fr * light.intensity * attenuation;
}
