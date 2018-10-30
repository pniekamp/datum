
//----------------------- Camera --------------------------------------------
//---------------------------------------------------------------------------
  
struct Camera
{
  vec3 position;
  float exposure;
  float focalwidth;
  float focaldistance;
  float skyboxlod;
  float ambientintensity;
  float specularintensity;
  float ssrstrength;
  float bloomstrength;
  vec4 fogdensity;
  uint frame;
};


///////////////////////// gamma /////////////////////////////////////////////
vec3 gamma(vec3 color)
{
  return pow(color, vec3(1/2.2));
}


///////////////////////// reinhard //////////////////////////////////////////
vec3 reinhard(vec3 color)
{
  return color / (color + vec3(1));
}


///////////////////////// filmic ////////////////////////////////////////////
vec3 filmic(vec3 color)
{
  vec3 x = max(color - 0.004, 0);

  return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
}


///////////////////////// filmic_ACES ///////////////////////////////////////
vec3 filmic_ACES(vec3 color)
{
  vec3 x = max(color, 0);

  return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
}


///////////////////////// filmic_uncharted2 /////////////////////////////////
vec3 filmic_uncharted2(vec3 color)
{
  const float A = 0.15;
  const float B = 0.50;
  const float C = 0.10;
  const float D = 0.20;
  const float E = 0.02;
  const float F = 0.30;
 
  vec3 x = max(color, 0);

  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}


///////////////////////// tonemap ///////////////////////////////////////////
vec3 tonemap(vec3 color)
{
//  return color;
//  return filmic(color);
//  return filmic_ACES(color);
  return filmic_uncharted2(2.0 * color) / filmic_uncharted2(vec3(11.2));
}


///////////////////////// colorgrad /////////////////////////////////////////
vec3 colorgrad(sampler3D colorlut, vec3 color)
{
  const vec3 size = vec3(32, 32, 32);//textureSize(colorlut, 0);
  const vec3 scale = (size - 1.0) / size;
  const vec3 offset = 1.0 / (2.0 * size);

  return texture(colorlut, scale * color + offset).rgb;
}
