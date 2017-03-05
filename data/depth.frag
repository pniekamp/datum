#version 450 core

layout(location=1) in vec3 normal;

layout(location=0) out vec4 fragrt0;
layout(location=1) out vec4 fragrt1;
layout(location=2) out vec4 fragnormal;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  fragrt0 = vec4(0);
  fragrt1 = vec4(0);
  fragnormal = vec4(0.5 * normal + 0.5, 1);
}
