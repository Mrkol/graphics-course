#version 430
#extension GL_GOOGLE_include_directive : require


#include "cpp_glsl_compat.h"


layout(push_constant) uniform params_t
{
  vec4 up;
  vec4 right;
  vec4 pos;
  mat4x4 viewProj;
} params;


void main(void)
{
  vec4 res;
  vec4 p = params.pos;
  vec4 u = params.up;
  vec4 r = params.right;

  if (gl_VertexIndex == 0) {
    res = p - r - u;
  } else if (gl_VertexIndex == 1) {
    res = p - r + u;
  } else if (gl_VertexIndex == 2) {
    res = p + r + u;
  } else if (gl_VertexIndex == 3) {
    res = p + r + u;
  } else if (gl_VertexIndex == 4) {
    res = p + r - u;
  } else {
    res = p - r - u;
  }

  res.w = 1;

  gl_Position = params.viewProj * res;
}