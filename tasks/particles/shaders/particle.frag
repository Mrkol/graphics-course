#version 430
#extension GL_GOOGLE_include_directive : require


#include "cpp_glsl_compat.h"


layout(location = 0) out vec4 out_fragColor;
layout(location = 0) in vec4 color;

void main() {
  out_fragColor = color;
}