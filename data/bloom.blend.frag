#version 450 core

layout(set=0, binding=2) uniform sampler2D src;

layout(location=0) in vec2 texcoord;

layout(location=0) out vec4 fragcolor;

void main()
{
  fragcolor = texture(src, texcoord);
}
