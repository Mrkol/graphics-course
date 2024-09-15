#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


#include "AmbientLight.h"
#include "SurfacePointInfo.h"
#include "BRDFCommon.h"


vec3 getColor(AmbientLight light, SurfacePointInfo info)
{
  return brdfCommon(info.normal, normalize(info.cameraVec), info.occlusion * light.colorIntensity.w * light.colorIntensity.rgb, info);
}

#define LIGHT_PARAMS_TYPE AmbientLight 
#define LIGHT_GET_COLOR getColor

#include "LightPassFragImpl.h"
