
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
#define BLENDSLICES 1

///////////////////////// poisson ///////////////////////////////////////////
float poisson(sampler2DArrayShadow shadowmap, vec4 texel, float spread)
{
  vec2 texelsize = 1.0 / textureSize(shadowmap, 0).xy;

  float sum = 0.0;

  for(int k = 0; k < 8; ++k)
  {
    sum += texture(shadowmap, vec4(texel.xy + spread*PoissonDisk[k]*texelsize, texel.z, texel.w));
  }

  return sum / 8;
}


///////////////////////// ambient_intensity /////////////////////////////////
float ambient_intensity(MainLight light, sampler2D ssaomap, vec2 uv)
{
  return texture(ssaomap, uv).x;
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
#if BLENDSLICES
     	float weight = max(4 * max(max(abs(shadowspace.x), abs(shadowspace.y)) - 0.75, 0.0), 500 * max(shadowspace.z - 0.998, 0.0));

      if (i+i < NSLICES && weight > 0.0)
      {
        vec4 shadowspace2 = shadowview[i+1] * vec4(position + bias[i+1] * normal, 1);
        vec4 texel2 = vec4(0.5 * shadowspace2.xy + 0.5, i+1, shadowspace2.z);
      
        return mix(poisson(shadowmap, texel, spread[i]), poisson(shadowmap, texel2, spread[i+1]), weight);
      }
#endif

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
float specular_intensity(vec3 N, vec3 E, vec3 L)
{
  //return max(dot(reflect(-L, N), E), 0.0);
  return max(dot(normalize(L + E), N), 0.0);
}


///////////////////////// main_light  ///////////////////////////////////////
void main_light(inout vec3 diffuse, inout vec3 specular, MainLight light, vec3 normal, vec3 eyevec, vec3 albedocolor, vec3 specularcolor, float specularexponent, float ambientocclusion, float shadowfactor)
{ 
  diffuse += 0.2 * ambientocclusion * light.intensity * albedocolor;
  
  diffuse += 0.8 * diffuse_intensity(normal, -light.direction) * light.intensity * albedocolor * shadowfactor;

  specular += pow(specular_intensity(normal, eyevec, -light.direction), specularexponent) * specularcolor * light.intensity * shadowfactor;
}


///////////////////////// point_light  //////////////////////////////////////
void point_light(inout vec3 diffuse, inout vec3 specular, PointLight light, vec3 position, vec3 normal, vec3 eyevec, vec3 albedocolor, vec3 specularcolor, float specularexponent)
{ 
  vec3 lightvec = normalize(light.position - position);
  
  float intensity = diffuse_intensity(normal, lightvec);

  float lightdistance = length(light.position - position);
  
  float attenuation = sign(intensity) / (light.attenuation.z + light.attenuation.y * lightdistance + light.attenuation.x * lightdistance * lightdistance);

  attenuation *= pow(clamp(1.0 - pow(lightdistance/light.attenuation.w, 4.0), 0.0, 1.0), 2.0);

  //attenuation *= min(10*light.attenuation.w/abs(light.position.z), 1.0);
  
  diffuse += intensity * light.intensity * albedocolor * attenuation;

  specular += pow(specular_intensity(normal, eyevec, lightvec), specularexponent) * specularcolor * light.intensity * attenuation;
}
