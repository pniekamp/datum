
//----------------------- Transform -----------------------------------------
//---------------------------------------------------------------------------

struct Transform
{
  vec4 real;
  vec4 dual;
};


///////////////////////// quaternion_conjugate //////////////////////////////
vec4 quaternion_conjugate(vec4 q)
{
  return vec4(q.x, -q.y, -q.z, -q.w);
}


///////////////////////// quaternion_multiply ///////////////////////////////
vec4 quaternion_multiply(vec4 q1, vec4 q2)
{
  float x = q1.x * q2.y + q1.y * q2.x + q1.z * q2.w - q1.w * q2.z;
  float y = q1.x * q2.z + q1.z * q2.x + q1.w * q2.y - q1.y * q2.w;
  float z = q1.x * q2.w + q1.w * q2.x + q1.y * q2.z - q1.z * q2.y;
  float w = q1.x * q2.x - q1.y * q2.y - q1.z * q2.z - q1.w * q2.w;

  return vec4(w, x, y, z);  
}


///////////////////////// quaternion_multiply ///////////////////////////////
vec3 quaternion_multiply(vec4 q, vec3 v)
{
  vec3 t = 2 * cross(q.yzw, v.xyz);
  
  return v + q.x * t + cross(q.yzw, t);
}


///////////////////////// transform_conjugate ///////////////////////////////
Transform transform_conjugate(Transform t)
{
  Transform result;
  
  result.real = vec4(t.real.x, -t.real.yzw);
  result.dual = vec4(-t.dual.x, t.dual.yzw);

  return result;
}


///////////////////////// transform_inverse /////////////////////////////////
Transform transform_inverse(Transform t)
{
  Transform result;
  
  result.real = vec4(t.real.x, -t.real.yzw);
  result.dual = vec4(t.dual.x, -t.dual.yzw);

  return result;
}

///////////////////////// transform_negate //////////////////////////////////
Transform transform_negate(Transform t)
{
  Transform result;
  
  result.real = -t.real;
  result.dual = -t.dual;

  return result;
}

///////////////////////// transform_normalize ///////////////////////////////
Transform transform_normalize(Transform t)
{
  Transform result;
  
  float norm = length(t.real);
  
  result.real = t.real / norm;
  result.dual = t.dual / norm;

  return result;
}

///////////////////////// transform_multiply ////////////////////////////////
Transform transform_multiply(Transform t1, Transform t2)
{
  Transform result;
  
  result.real = quaternion_multiply(t1.real, t2.real);
  result.dual = quaternion_multiply(t1.real, t2.dual) + quaternion_multiply(t1.dual, t2.real);

  return result;
}


///////////////////////// transform_multiply ////////////////////////////////
vec3 transform_multiply(Transform t, vec3 v)
{
  Transform pt = { vec4(1, 0, 0, 0), vec4(0, v.x, v.y, v.z) };
  
  return transform_multiply(transform_multiply(t, pt), transform_conjugate(t)).dual.yzw;
}


///////////////////////// transform_matrix //////////////////////////////////
mat4 transform_matrix(Transform transform)
{
  mat4 result;
  
  vec4 real = transform.real;
  vec4 dual = transform.dual;

  vec3 shift = 2 * quaternion_multiply(dual, quaternion_conjugate(real)).yzw;

  result[0][0] = 1 - 2*real.z*real.z - 2*real.w*real.w;
  result[0][1] = 2*real.y*real.z + 2*real.w*real.x;
  result[0][2] = 2*real.y*real.w - 2*real.z*real.x;
  result[0][3] = 0;
  result[1][0] = 2*real.y*real.z - 2*real.w*real.x;
  result[1][1] = 1 - 2*real.y*real.y - 2*real.w*real.w;
  result[1][2] = 2*real.z*real.w + 2*real.y*real.x;
  result[1][3] = 0;
  result[2][0] = 2*real.y*real.w + 2*real.z*real.x;
  result[2][1] = 2*real.z*real.w - 2*real.y*real.x;
  result[2][2] = 1 - 2*real.y*real.y - 2*real.z*real.z;
  result[2][3] = 0;
  result[3][0] = shift.x;
  result[3][1] = shift.y;
  result[3][2] = shift.z;
  result[3][3] = 1;

  return result;
}


///////////////////////// transform_blend ///////////////////////////////////
Transform transform_blend(vec4 weights, Transform t1, Transform t2, Transform t3, Transform t4)
{
  Transform result;

  if (dot(t1.real, t2.real) < 0) weights[1] *= -1.0;
  if (dot(t1.real, t3.real) < 0) weights[2] *= -1.0;
  if (dot(t1.real, t4.real) < 0) weights[3] *= -1.0;

  result.real = weights[0]*t1.real + weights[1]*t2.real + weights[2]*t3.real + weights[3]*t4.real;
  result.dual = weights[0]*t1.dual + weights[1]*t2.dual + weights[2]*t3.dual + weights[3]*t4.dual;
  
  result = transform_normalize(result); 

  return result;
}


///////////////////////// transform_bend ////////////////////////////////////
vec3 transform_bend(vec3 v, vec3 wind, vec3 scale)
{
  float bf = dot(v, scale);

  bf += 1.0;
  bf *= bf;
  bf = bf * bf - bf;
  
  return normalize(v + wind * bf) * length(v);
}


///////////////////////// transform_detailbend //////////////////////////////
vec3 transform_detailbend(vec3 v, vec3 pos, float time, vec3 wind, vec3 scale)
{
  float phase = dot(v, vec3(pos.x + pos.y + pos.z));

  vec2 waves = (fract((time + phase) * vec2(1.975, 0.793)) * 2.0 - 1.0);

  waves = abs(fract(waves + 0.5) * 2.0 - 1.0);
  waves = waves * waves * (3.0 - 2.0 * waves);

  float wavesum = waves.x + waves.y;

  return v + wind * wavesum * dot(v, scale);
}


///////////////////////// map_parabolic /////////////////////////////////////
vec4 map_parabolic(vec4 position)
{
  float L = length(position.xyz);
  
  vec3 P = position.xyz / L;

  return vec4(P.xy / (1 - P.z), L / 1000, 1);
}
