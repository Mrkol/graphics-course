#version 430
#extension GL_GOOGLE_include_directive : require


#include "cpp_glsl_compat.h"


layout(push_constant) uniform params_t
{
  vec4 camPos;
  mat4x4 viewProj;
} params;


struct QuadParams {
  vec4 posAndAngle;
  vec4 color;
};

layout(set=0, binding=0) readonly buffer quadParams{
  QuadParams quad_params[];
};


layout(location = 0) out vec4 color;


void main(void)
{
  color = quad_params[gl_InstanceIndex].color;

  vec4 res;
  vec4 p = quad_params[gl_InstanceIndex].posAndAngle;
  vec3 look = params.camPos.xyz - p.xyz;
  vec4 r_notRotated = vec4(normalize(cross(vec3(0, 1, 0), look)) * 0.02, 1);
  vec4 u_notRotated = vec4(normalize(cross(r_notRotated.xyz, look)) * 0.02, 1);

  float angle = quad_params[gl_InstanceIndex].posAndAngle.w;
  vec4 r = r_notRotated * cos(angle) + u_notRotated * sin(angle);
  vec4 u = u_notRotated * cos(angle) - r_notRotated * sin(angle);

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