#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


layout(location = 0) out vec4 out_fragColor;
layout(location = 1) out vec4 out_fragNormal;
layout(location = 2) out float out_fragWc;

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} surf;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  mat4 mModel;
  uint relemIdx;
} params;


const vec2 resolution = vec2(1280, 720);

vec3 getPos() {
  return vec3(
    (2 * gl_FragCoord.x / resolution.x) - 1,
    (2 * gl_FragCoord.y / resolution.y) - 1,
    gl_FragCoord.z
  ) / gl_FragCoord.w;
}


void main()
{
  out_fragColor = vec4(1., 1., 1., 0) * 0.3;
  out_fragNormal.rgb = surf.wNorm;
  out_fragWc = gl_FragCoord.w;  
}
