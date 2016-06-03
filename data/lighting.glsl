
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
  float smoothness;
  float reflectivity;
  float alpha;
};


//----------------------- Poisson Disk --------------------------------------
//---------------------------------------------------------------------------

const vec2 PoissonDisk[8] = { 
  vec2(-0.168855, -0.446941),
  vec2(-0.645265, 0.0602261),
  vec2(-0.841704, -0.468692),
  vec2(0.296674, 0.437577),
  vec2(0.389539, -0.806041),
  vec2(-0.298654, 0.561369),
  vec2(0.910996, -0.295452),
  vec2(0.379805, -0.158868)
};


//----------------------- Functions -----------------------------------------
//---------------------------------------------------------------------------

#define NSLICES 4
#define PI 3.1415926535897932384626433832795


///////////////////////// poisson ///////////////////////////////////////////
float poisson(sampler2DArrayShadow shadowmap, vec4 texel, float spread)
{
  vec2 texelsize = spread / textureSize(shadowmap, 0).xy;

  float sum = 0.0;

  for(int k = 0; k < 8; ++k)
  {
    sum += texture(shadowmap, vec4(texel.xy + PoissonDisk[k]*texelsize, texel.z, texel.w));
  }

  return sum / 8;
}


///////////////////////// unpack_material ///////////////////////////////////
Material unpack_material(vec4 rt0, vec4 rt1)
{
  Material material;
  
  material.albedo = rt0.rgb;
  material.emissive = rt0.a;
  material.metalness = rt1.r;
  material.smoothness = rt1.a;
  material.reflectivity = rt1.g;
  
  material.diffuse = material.albedo * (1 - material.metalness);
  material.specular = mix(vec3(0.16 * material.reflectivity * material.reflectivity), material.albedo, material.metalness);
  
  material.alpha = (1 - material.smoothness) * (1 - material.smoothness);

  return material;
}


///////////////////////// ambient_intensity /////////////////////////////////
float ambient_intensity(MainLight light, sampler2DArray ssaomap, vec2 uv)
{
  return texture(ssaomap, vec3(uv, 0)).x;
}


///////////////////////// shadow_intensity //////////////////////////////////
float shadow_intensity(MainLight light, mat4 shadowview[NSLICES], sampler2DArrayShadow shadowmap, vec3 position, vec3 normal)
{
  const float bias[NSLICES] = { 0.05, 0.06, 0.10, 0.25 };
  const float spread[NSLICES] = { 1.5, 1.2, 1.0, 0.2 };

  for(int i = 0; i < NSLICES; ++i)
  {
    vec4 shadowspace = shadowview[i] * vec4(position + bias[i] * normal, 1);
    
    vec4 texel = vec4(0.5 * shadowspace.xy + 0.5, i, shadowspace.z);

    if (texel.x > 0.0 && texel.x < 1.0 && texel.y > 0.0 && texel.y < 1.0 && texel.w > 0.0 && texel.w < 1.0)
    {    
     	float weight = max(4 * max(max(abs(shadowspace.x), abs(shadowspace.y)) - 0.75, 0.0), 500 * max(shadowspace.z - 0.998, 0.0));

      if (i+i < NSLICES && weight > 0.0)
      {
        vec4 shadowspace2 = shadowview[i+1] * vec4(position + bias[i+1] * normal, 1);
        vec4 texel2 = vec4(0.5 * shadowspace2.xy + 0.5, i+1, shadowspace2.z);
      
        return mix(poisson(shadowmap, texel, spread[i]), poisson(shadowmap, texel2, spread[i+1]), weight);
      }

      return poisson(shadowmap, texel, spread[i]);
    }
  }
  
  return 1.0;
}


///////////////////////// diffuse_intensity /////////////////////////////////
float diffuse_intensity(vec3 N, vec3 L)
{
  return max(dot(N, L), 0.0);
}

///////////////////////// specular_intensity ////////////////////////////////
float specular_intensity(vec3 N, vec3 V, vec3 L)
{
  //return max(dot(reflect(-L, N), V), 0.0);
  return max(dot(normalize(L + V), N), 0.0);
}


///////////////////////// specular_dominantdirection ////////////////////////
vec3 specular_dominantdirection(vec3 N, vec3 R, float smoothness)
{
  float roughness = 1 - smoothness;

  return mix(N, R, smoothness * (sqrt(smoothness) + roughness));
}

///////////////////////// dffuse_dominantdirection //////////////////////////
vec3 dffuse_dominantdirection(vec3 N, vec3 V, float smoothness)
{
  float roughness = 1 - smoothness;  
  float a = 1.02341f * roughness - 1.51174f;
  float b = -0.511705f * roughness + 0.755868f;  
  
  return mix(N, V, clamp((dot(N, V) * a + b) * roughness, 0.0, 1.0));
}


///////////////////////// fresnel_schlick ///////////////////////////////////
vec3 fresnel_schlick(vec3 f0, float f90, float u)
{
  return f0 + (f90 - f0) * pow(1.0f - u, 5.0f);
}

///////////////////////// visibility_smith //////////////////////////////////
float visibility_smith(float NdotL, float NdotV, float alpha)
{
  float k = alpha / 2;
  float GGXL = NdotL * (1-k) + k;
  float GGXV = NdotV * (1-k) + k;

  return 0.25f / (GGXV * GGXL + 1e-5);
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
  
  float lightscatter = fresnel_schlick(vec3(1.0f), f90, NdotL).r;
  float viewscatter = fresnel_schlick(vec3(1.0f), f90, NdotV).r;

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
void env_light(inout vec3 diffuse, inout vec3 specular, Material material, vec3 envdiffuse, vec3 envspecular, vec2 envbrdf)
{ 
  float f90 = 0.8f;
  
  diffuse += envdiffuse * material.diffuse;
  specular += envspecular * (material.specular * envbrdf.x + f90 * envbrdf.y);
}


///////////////////////// main_light  ///////////////////////////////////////
void main_light(inout vec3 diffuse, inout vec3 specular, MainLight light, vec3 normal, vec3 eyevec, Material material, float ambientocclusion, float shadowfactor)
{ 
  vec3 lightvec = -light.direction;

  vec3 halfvec = normalize(lightvec + eyevec);

	float NdotV = max(dot(normal, eyevec), 0.0);
	float NdotL = max(dot(normal, lightvec), 0.0);
	float NdotH = max(dot(normal, halfvec), 0.0);
	float LdotH = max(dot(lightvec, halfvec), 0.0);

  float Fd = diffuse_disney(NdotV, NdotL, LdotH, material.alpha) / PI;

  vec3 Fr = specular_ggx(material.specular, 1, NdotV, NdotL, LdotH, NdotH, material.alpha) / PI;

  diffuse += NdotL * Fd * light.intensity * shadowfactor;

  specular += NdotL * Fr * light.intensity * shadowfactor;
}


///////////////////////// point_light  //////////////////////////////////////
void point_light(inout vec3 diffuse, inout vec3 specular, PointLight light, vec3 position, vec3 normal, vec3 eyevec, Material material)
{ 
  vec3 lightvec = normalize(light.position - position); 
  
  vec3 halfvec = normalize(lightvec + eyevec);

	float NdotV = max(dot(normal, eyevec), 0.0);
	float NdotL = max(dot(normal, lightvec), 0.0);
	float NdotH = max(dot(normal, halfvec), 0.0);
	float LdotH = max(dot(lightvec, halfvec), 0.0);

  float Fd = diffuse_disney(NdotV, NdotL, LdotH, material.alpha) / PI;

  vec3 Fr = specular_ggx(material.specular, 1, NdotV, NdotL, LdotH, NdotH, material.alpha) / PI;

  float lightdistance = length(light.position - position);
  
  float attenuation = sign(NdotL) / (light.attenuation.z + light.attenuation.y * lightdistance + light.attenuation.x * lightdistance * lightdistance);

  attenuation *= pow(clamp(1.0 - pow(lightdistance/light.attenuation.w, 4.0), 0.0, 1.0), 2.0);
  
  diffuse += NdotL * Fd * light.intensity * attenuation;

  specular += NdotL * Fr * light.intensity * attenuation;
}
