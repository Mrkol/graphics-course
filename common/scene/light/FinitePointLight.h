#ifndef FINITE_POINT_LIGHT_H_INCLUDED
#define FINITE_POINT_LIGHT_H_INCLUDED

#include "cpp_glsl_compat.h"

struct FinitePointLight
{
  shader_vec4 positionRange;
  shader_vec4 colorIntensity;
};


#endif
