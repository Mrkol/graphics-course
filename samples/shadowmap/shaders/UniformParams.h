#ifndef UNIFORM_PARAMS_H_INCLUDED
#define UNIFORM_PARAMS_H_INCLUDED

#include "cpp_glsl_compat.h"


struct UniformParams
{
  shader_mat4  lightMatrix;
  shader_vec3  lightPos;
  shader_float time;
  shader_vec3  baseColor;
};


#endif // UNIFORM_PARAMS_H_INCLUDED
