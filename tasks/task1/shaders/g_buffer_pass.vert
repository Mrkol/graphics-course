#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.glsl"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(location = 0) out VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} vOut;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  mat4 mModel;
  vec4 baseColorMetallicFactor;
  vec4 emissiveRoughnessFactors;
} params;

void main()
{
  const vec4 wNorm = vec4(decode_normal(floatBitsToInt(vPosNorm.w)), 0.0);
  const vec4 wTang = vec4(decode_normal(floatBitsToInt(vTexCoordAndTang.z)), 0.0);

  vOut.wPos = (params.mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
  vOut.wNorm = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);
  vOut.wTangent = normalize(mat3(transpose(inverse(params.mModel))) * wTang.xyz);
  vOut.texCoord = vTexCoordAndTang.xy;

  gl_Position = params.mProjView * vec4(vOut.wPos, 1.0);
}
