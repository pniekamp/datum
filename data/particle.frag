#version 440 core
#include "gbuffer.glsl"
#include "camera.glsl"

layout(constant_id = 28) const bool SoftParticles = true;

layout(origin_upper_left) in vec4 gl_FragCoord;

layout(set=0, binding=0, std430, row_major) readonly buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;
  mat4 prevview;
  mat4 skyview;
  vec4 viewport;

  Camera camera;

} scene;

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(set=0, binding=11, input_attachment_index=3) uniform subpassInput depthmap;

layout(location=0) in vec3 texcoord;
layout(location=1) flat in vec4 tint;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 color = texture(albedomap, texcoord) * tint;

  if (SoftParticles)
  {
    float depth = subpassLoad(depthmap).r;
  
    color *= clamp(0.6 * (view_depth(scene.proj, depth) - gl_FragCoord.z/gl_FragCoord.w), 0, 1);
  }

  if (color.a < 0.003)
    discard;

  fragcolor = vec4(scene.camera.exposure * 4.0f * color.rgb, color.a); 
}
