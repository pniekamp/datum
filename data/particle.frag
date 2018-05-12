#version 440 core
#include "gbuffer.glsl"
#include "camera.glsl"

layout(constant_id = 28) const bool SoftParticles = true;

layout(set=0, binding=0, std430, row_major) readonly buffer SceneSet 
{
  mat4 proj;
  mat4 invproj;
  mat4 view;
  mat4 invview;
  mat4 worldview;
  mat4 orthoview;
  mat4 prevview;
  mat4 skyview;
  vec4 viewport;

  Camera camera;

} scene;

layout(set=1, binding=1) uniform sampler2DArray albedomap;

layout(set=0, binding=11, input_attachment_index=0) uniform subpassInput depthmap;

layout(location=0) in vec3 texcoord;
layout(location=1) in vec4 lighting;

layout(location=0) out vec4 fragcolor;

#ifdef WEIGHTEDBLEND
layout(location=1) out vec4 fragweight;
#endif

///////////////////////// main //////////////////////////////////////////////
void main()
{
  vec4 color = texture(albedomap, texcoord) * lighting;

  if (SoftParticles)
  {
    color *= clamp(0.6 * (view_depth(scene.proj, subpassLoad(depthmap).r) - view_depth(scene.proj, gl_FragCoord.z)), 0, 1);
  }

#ifdef WEIGHTEDBLEND

  //float weight = color.a * max(3e3 * pow(1 - gl_FragCoord.z, 3), 1e-2);
  float weight = color.a * max(3e3 * (1 - pow(gl_FragCoord.z, 3)), 1e-2);
  //float weight = color.a * clamp(0.03 / (1e-5 + pow(view_depth(scene.proj, gl_FragCoord.z)/200, 4)), 1e-2, 3e3);

  fragcolor = vec4(scene.camera.exposure * color.rgb * weight, color.a);
  fragweight = vec4(color.a * weight);
  
#else  

  fragcolor = vec4(scene.camera.exposure * 4.0 * color.rgb, color.a);

#endif 
}
