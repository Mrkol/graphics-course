#ifndef SHADER_COMMON_H_INCLUDED
#define SHADER_COMMON_H_INCLUDED

// GLSL-C++ datatype compatibility layer

#ifdef __cplusplus

#include <glm/glm.hpp>

// NOTE: This is technically completely wrong,
// as GLSL words are guaranteed to be 32-bit,
// while C++ unsigned int can be 16-bit.
using shader_uint  = glm::uint;
using shader_uvec2 = glm::uvec2;
using shader_uvec3 = glm::uvec3;

using shader_float = float;
using shader_vec2  = glm::vec2;
using shader_vec3  = glm::vec3;
using shader_vec4  = glm::vec4;
using shader_mat4  = glm::mat4x4;

// The funny thing is, on a GPU, you might as well consider
// a single byte to be 32 bits, because nothing can be smaller
// than 32 bits, so a bool has to be 32 bits as well.
using shader_bool  = glm::uint;

#else

#define shader_uint  uint
#define shader_uvec2 uvec2

#define shader_float float
#define shader_vec2  vec2
#define shader_vec3  vec3
#define shader_vec4  vec4
#define shader_mat4  mat4

#define shader_bool  bool

#endif


struct UniformParams
{
  shader_mat4  lightMatrix;
  shader_vec3  lightPos;
  shader_float time;
  shader_vec3  baseColor;
  shader_bool  animateLightColor;
};

#endif // SHADER_COMMON_H_INCLUDED
