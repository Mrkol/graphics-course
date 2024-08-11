#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "shader_common.h"

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams params;
};

layout (binding = 1) uniform sampler2D shadowMap;

void main()
{
  const vec4 posLightClipSpace = params.lightMatrix*vec4(surf.wPos, 1.0f);

  // for orto matrix, we don't need perspective division, you can remove it if you want; this is general case;
  const vec3 posLightSpaceNDC = posLightClipSpace.xyz/posLightClipSpace.w;

  // just shift coords from [-1,1] to [0,1]
  const vec2 shadowTexCoord = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);

  const bool  outOfView = (shadowTexCoord.x < 0.0001f || shadowTexCoord.x > 0.9999f || shadowTexCoord.y < 0.0091f || shadowTexCoord.y > 0.9999f);
  const float shadow    = ((posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord, 0).x + 0.001f) || outOfView) ? 1.0f : 0.0f;

  const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
  const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);

  vec4 lightColor1 = mix(dark_violet, chartreuse, abs(sin(params.time)));
  vec4 lightColor2 = vec4(1.0f, 1.0f, 1.0f, 1.0f);

  vec3 lightDir   = normalize(params.lightPos - surf.wPos);
  vec4 lightColor = max(dot(surf.wNorm, lightDir), 0.0f) * lightColor1;
  const float ambient = 0.05;
  // Light is formula pretty arbitrary and most definitely wrong
  out_fragColor = (lightColor * (shadow + ambient)) * vec4(params.baseColor, 1.0f);
}
