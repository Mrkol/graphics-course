#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require

#include "unpack_attributes.glsl"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
} params;

struct Draw {
  mat4 mModel;
  int relemIdx;
  int padding[1];
};
layout(set=2, binding=0) readonly buffer Draws {
  Draw draws[];
};

layout (location = 0 ) out VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
  flat int drawID;
} vOut;

out gl_PerVertex { vec4 gl_Position; };

void main(void)
{
  // const vec4 wNorm = vec4(decode_normal(floatBitsToInt(vPosNorm.w)),     0.0f);
  // const vec4 wTang = vec4(decode_normal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);
  const vec4 wNorm = vec4(decode_normal_compressed(floatBitsToInt(vPosNorm.w)),     0.0f);
  const vec4 wTang = vec4(decode_normal_compressed(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

  mat4 mProjView = params.mProjView;
  mat4 mModel = draws[gl_DrawID].mModel;

  vOut.wPos   = (mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
  vOut.wNorm  = normalize(mat3(transpose(inverse(mModel))) * wNorm.xyz);
  vOut.wTangent = normalize(mat3(transpose(inverse(mModel))) * wTang.xyz);
  vOut.texCoord = vTexCoordAndTang.xy;
  vOut.drawID = gl_DrawID;

  gl_Position = mProjView * vec4(vOut.wPos, 1.0);
}
