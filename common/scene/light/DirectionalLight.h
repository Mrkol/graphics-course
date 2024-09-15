#ifndef DIRECT_LIGHT_H_INCLUDED
#define DIRECT_LIGHT_H_INCLUDED

#include "cpp_glsl_compat.h"

struct DirectionalLight
{
  shader_vec4 direction;
  shader_vec4 colorIntensity;
};


#endif
