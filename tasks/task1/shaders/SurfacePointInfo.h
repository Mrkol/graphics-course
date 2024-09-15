#ifndef SURFACE_POINT_INFO_H
#define SURFACE_POINT_INFO_H

struct SurfacePointInfo
{
  vec3 baseColor;
  vec3 normal;
  vec3 emissive;

  float occlusion;
  float metallic;
  float roughness;

  vec3 position;

  vec3 cameraVec;
};

#endif
