#ifndef UNIFORM_PARAMS_H_INCLUDED
#define UNIFORM_PARAMS_H_INCLUDED

#include "cpp_glsl_compat.h"

struct UniformParams
{
  shader_mat4  lightMatrix;
  shader_vec4  lightPos;
  shader_vec3  lightColor;
  shader_float gamma;
  shader_float exposure;
  shader_float time;
  shader_float debugMode;
  shader_vec3  baseColor;
  shader_float _pad0;
  float shadowBias;
  float shadowPCF;
};

#endif
