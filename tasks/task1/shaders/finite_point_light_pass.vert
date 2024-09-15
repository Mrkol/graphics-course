#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "FinitePointLight.h"

const vec3 vertices[] = vec3[](
  vec3(-1.0, -1.0, -1.0),
  vec3(-1.0, -1.0, 1.0),
  vec3(-1.0, 1.0, -1.0),
  vec3(-1.0, 1.0, 1.0),
  vec3(1.0, -1.0, -1.0),
  vec3(1.0, -1.0, 1.0),
  vec3(1.0, 1.0, -1.0),
  vec3(1.0, 1.0, 1.0));

const uint indices[] = uint[](
  0, 2, 3, 0, 3, 1, // X-
  4, 5, 7, 4, 7, 6, // X+
  0, 1, 5, 0, 5, 4, // Y-
  2, 6, 7, 2, 7, 3, // Y+
  0, 4, 6, 0, 6, 2, // Z-
  1, 3, 7, 1, 7, 5 // Z+
);

vec3 getVertex(FinitePointLight light)
{
  return vertices[indices[gl_VertexIndex]] * light.positionRange.w + light.positionRange.xyz;
}

#define LIGHT_PARAMS_TYPE FinitePointLight
#define LIGHT_GET_VERTEX getVertex
#define LIGHT_VERTEX_IN_WORLD_SPACE

#include "LightPassVertImpl.h"
