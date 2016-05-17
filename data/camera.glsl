
//----------------------- Camera --------------------------------------------
//---------------------------------------------------------------------------

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
  vec3 x = max(vec3(0.0), color - 0.004);

  return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
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
 
  return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}


///////////////////////// whitepoint ////////////////////////////////////////
vec3 whitepoint()
{
  return filmic_uncharted2(vec3(11.2));
}


///////////////////////// tonemap ///////////////////////////////////////////
vec3 tonemap(vec3 color)
{
  return filmic_uncharted2(2.0 * color) / whitepoint();
}
