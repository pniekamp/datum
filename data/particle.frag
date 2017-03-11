#version 440 core
#include "gbuffer.glsl"
#include "camera.glsl"

layout(constant_id = 46) const uint ShadowSlices = 4;
layout(constant_id = 29) const uint MaxPointLights = 256;
layout(constant_id = 31) const uint MaxEnvironments = 6;
layout(constant_id = 28) const bool SoftParticles = true;

layout(origin_upper_left) in vec4 gl_FragCoord;

layout(std430, set=0, binding=0, row_major) buffer SceneSet 
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

layout(set=0, binding=4) uniform sampler2D depthmap;

layout(location=0) in vec3 texcoord;
layout(location=1) flat in vec4 tint;

layout(location=0) out vec4 fragcolor;

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 color = texture(albedomap, texcoord) * tint;

  if (SoftParticles)
  {
    float depth = texelFetch(depthmap, ivec2(gl_FragCoord.xy), 0).r;
  
    color *= clamp(0.6 * (view_depth(scene.proj, depth) - gl_FragCoord.z/gl_FragCoord.w), 0, 1);
  }

  if (color.a < 0.003)
    discard;

  fragcolor = vec4(scene.camera.exposure * 4.0f * color.rgb, color.a); 
}
