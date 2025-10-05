#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"

layout(location = 0) out vec4 out_fragColor;
layout(location = 0) in VS_OUT {
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData { UniformParams params; };
layout(binding = 1) uniform sampler2D shadowMap;

vec3 tonemap(vec3 c, float exposure, float gamma) {
  c *= exposure;
  c = c / (c + vec3(1.0));
  c = pow(c, vec3(1.0 / max(gamma, 1e-3)));
  return c;
}

float pcfShadow(sampler2D smap, vec2 uv, float compare, float bias, float radius)
{
    vec2 texel = 1.0 / vec2(textureSize(smap, 0));
    vec2 step  = texel * radius;
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x) {
        float d = textureLod(smap, uv + vec2(x, y) * step, 0.0).r;
        sum += ((compare - bias) < d) ? 1.0 : 0.0;
    }
    return sum / 9.0;
}

void main()
{
  vec4 posLightClipSpace = params.lightMatrix * vec4(surf.wPos, 1.0);
  vec3 posLightNDC = posLightClipSpace.xyz / posLightClipSpace.w;
  vec2 shadowTexCoord = posLightNDC.xy * 0.5 + vec2(0.5);
  bool outOfView = (shadowTexCoord.x <= 0.0001 || shadowTexCoord.x >= 0.9999 ||
                    shadowTexCoord.y <= 0.0001 || shadowTexCoord.y >= 0.9999);
  float minBias    = params.shadowBias;
  float slopeScale = 2.0 * params.shadowBias;
  float dzdx = dFdx(posLightNDC.z);
  float dzdy = dFdy(posLightNDC.z);
  float biasVal = minBias + slopeScale * max(abs(dzdx), abs(dzdy));
  const float PCF_RADIUS = 0.60;
  float shadow = outOfView ? 1.0 : pcfShadow(shadowMap, shadowTexCoord, posLightNDC.z, biasVal, PCF_RADIUS);
  vec3  L = normalize(params.lightPos.xyz - surf.wPos);
  float NdotL = max(dot(normalize(surf.wNorm), L), 0.0);
  vec3  lightColor = params.lightColor * params.lightPos.w;

  if (params.debugMode == 1) {
    vec3 n = normalize(surf.wNorm);
    out_fragColor = vec4(n * 0.5 + 0.5, 1.0);
    return;
  }

  if (params.debugMode == 2) {
    out_fragColor = vec4(params.baseColor, 1.0);
    return;
  }

  float ambient = 0.05;
  vec3  lit = (NdotL * lightColor) * shadow + ambient;
  vec3  color = tonemap(lit * params.baseColor, params.exposure, params.gamma);

  out_fragColor = vec4(color, 1.0);
}
