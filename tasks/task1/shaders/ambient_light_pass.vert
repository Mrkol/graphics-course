#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "AmbientLight.h"

const vec2 vertices[] = vec2[](vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));

const uint indices[] = uint[](0, 1, 3, 0, 3, 2);

vec3 getVertex(AmbientLight light)
{
  return vec3(vertices[indices[gl_VertexIndex]], 0.0);
}

#define LIGHT_PARAMS_TYPE AmbientLight
#define LIGHT_GET_VERTEX getVertex

#include "LightPassVertImpl.h"
