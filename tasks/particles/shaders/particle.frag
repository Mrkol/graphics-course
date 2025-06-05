#version 430
#extension GL_GOOGLE_include_directive : require


#include "cpp_glsl_compat.h"


layout(location = 0) out vec4 out_fragColor;

layout(push_constant) uniform params_t
{
  vec4 up;
  vec4 right;
  vec4 pos;
  mat4x4 viewProj;
} params;


void main() {
  out_fragColor = vec4(1.0f);
}