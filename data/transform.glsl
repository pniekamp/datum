
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
  Transform pt = { vec4(1, 0, 0, 0), {0.0f, v.x, v.y, v.z} };
  
  return transform_multiply(transform_multiply(t, pt), transform_conjugate(t)).dual.yzw;
}


///////////////////////// transform_matrix //////////////////////////////////
mat4 transform_matrix(Transform transform)
{
  mat4 result;
  
  vec4 real = transform.real;
  vec4 dual = transform.dual;

  vec3 shift = 2.0 * quaternion_multiply(dual, quaternion_conjugate(real)).yzw;

  result[0][0] = 1 - 2*real.z*real.z - 2*real.w*real.w;
  result[0][1] = 2*real.y*real.z + 2*real.w*real.x;
  result[0][2] = 2*real.y*real.w - 2*real.z*real.x;
  result[0][3] = 0.0f;
  result[1][0] = 2*real.y*real.z - 2*real.w*real.x;
  result[1][1] = 1 - 2*real.y*real.y - 2*real.w*real.w;
  result[1][2] = 2*real.z*real.w + 2*real.y*real.x;
  result[1][3] = 0.0f;
  result[2][0] = 2*real.y*real.w + 2*real.z*real.x;
  result[2][1] = 2*real.z*real.w - 2*real.y*real.x;
  result[2][2] = 1 - 2*real.y*real.y - 2*real.z*real.z;
  result[2][3] = 0.0f;
  result[3][0] = shift.x;
  result[3][1] = shift.y;
  result[3][2] = shift.z;
  result[3][3] = 1.0f;

  return result;
}
