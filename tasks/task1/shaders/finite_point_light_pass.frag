#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


#include "FinitePointLight.h"
#include "SurfacePointInfo.h"
#include "BRDFCommon.h"


vec3 getColor(FinitePointLight light, SurfacePointInfo info)
{
  vec3 lightVec = light.positionRange.xyz - info.position;
  float lightDistance = length(lightVec);
  vec3 lightDir = normalize(lightVec);

  float attenuation = clamp(1.0 - pow(lightDistance / light.positionRange.w, 4.0), 0.0, 1.0) / (lightDistance * lightDistance);

  return brdfCommon(lightDir, normalize(info.cameraVec), attenuation * info.occlusion * light.colorIntensity.w * light.colorIntensity.rgb, info);
}

#define LIGHT_PARAMS_TYPE FinitePointLight
#define LIGHT_GET_COLOR getColor

#include "LightPassFragImpl.h"
