#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 out_fragColor;
layout(location = 1) out vec4 out_fragNormal;
layout(location = 2) out float out_fragWc;

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
  vec4 normal;
  float height;
} surf;

layout(binding = 0) uniform sampler2D hmap;

float depthToDist(float depth)
{
  return depth / gl_FragCoord.w;
}

vec3 heightColor(float height)
{
  if(height > 25)
    return vec3(0.9, 0.9, 0.91);
  if(height < 8.01)
    return vec3(0.01, 0.01, 0.71);
  if(height < 8.1)
    return vec3(0.81, 0.83, 0.23);

  return vec3(0.08, 0.31, 0.09);
}

void main(void)
{
  //out_fragColor = fract(surf.texCoord.x * 64.) < 0.5 ? vec4(1., 0., 0., 0) : vec4(0., 1., 0., 0);
  //out_fragColor = vec4(gl_FragCoord.z, depthToDist(gl_FragCoord.z) / far, 1., 0);
  out_fragColor.rgb = heightColor(surf.height);

  out_fragNormal = vec4(surf.normal.rgb, 0);
  out_fragWc = gl_FragCoord.w;
}