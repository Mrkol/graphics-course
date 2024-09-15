#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


#include "DirectionalLight.h"
#include "SurfacePointInfo.h"
#include "BRDFCommon.h"


vec3 getColor(DirectionalLight light, SurfacePointInfo info)
{
  vec3 lightDir = light.direction.xyz;

  return brdfCommon(lightDir, normalize(info.cameraVec), info.occlusion * light.colorIntensity.w * light.colorIntensity.rgb, info);
}

#define LIGHT_PARAMS_TYPE DirectionalLight
#define LIGHT_GET_COLOR getColor

#include "LightPassFragImpl.h"
