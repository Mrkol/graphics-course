#include "UniformParams.h"

struct VS_OUT
{
  [[vk::location(0)]]
  float3 wPos : TEXCOORD0;
  [[vk::location(1)]]
  float3 wNorm : TEXCOORD1;
  // Keep locations and semantics in sync with simple.vert.
  [[vk::location(2)]]
  float3 wTangent : TEXCOORD2;
  [[vk::location(3)]]
  float2 texCoord : TEXCOORD3;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UniformParams> params;

// HLSL declares texture and sampler resources separately.
[[vk::binding(1, 0)]]
Texture2D<float> shadowMap;
[[vk::binding(1, 0)]]
SamplerState shadowMap_samplerstate;

float4 main(VS_OUT surf) : SV_Target
{
  // HLSL: do not use * operator. It is used for per-component multiplication
  const float4 posLightClipSpace = mul(params.lightMatrix, float4(surf.wPos, 1.0f));

  // for orto matrix, we don't need perspective division, you can remove it if you want; this is general case;
  const float3 posLightSpaceNDC = posLightClipSpace.xyz/posLightClipSpace.w;

  // just shift coords from [-1,1] to [0,1]
  const float2 shadowTexCoord = posLightSpaceNDC.xy*0.5f + float2(0.5f, 0.5f);

  const bool  outOfView = (shadowTexCoord.x < 0.0001f || shadowTexCoord.x > 0.9999f || shadowTexCoord.y < 0.0091f || shadowTexCoord.y > 0.9999f);
  const float shadow    = ((posLightSpaceNDC.z < shadowMap.SampleLevel(shadowMap_samplerstate, shadowTexCoord, 0).x + 0.001f) || outOfView) ? 1.0f : 0.0f;

  const float4 dark_violet = float4(0.59f, 0.0f, 0.82f, 1.0f);
  const float4 chartreuse  = float4(0.5f, 1.0f, 0.0f, 1.0f);

  // HLSL: lerp is identical to GLSL's mix function.
  // Inverse order of lerped colors is used to demonstrate diff with GLSL version
  const float4 lightColor1 = lerp(chartreuse, dark_violet, abs(sin(params.time)));
  const float4 lightColor2 = float4(1.0f, 1.0f, 1.0f, 1.0f);

  const float3 lightDir   = normalize(params.lightPos - surf.wPos);
  const float4 lightColor = max(dot(surf.wNorm, lightDir), 0.0f) * lightColor1;
  const float ambient = 0.05;
  // Light formula is pretty arbitrary and most definitely wrong
  return (lightColor * shadow + ambient) * float4(params.baseColor, 1.0f);
}
