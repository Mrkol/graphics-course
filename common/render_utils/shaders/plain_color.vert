#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vColor;

layout(location = 0) out VS_OUT
{
  vec3 color;
} vOut;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
} params;

void main()
{
  vOut.color = vColor;

  gl_Position = params.mProjView * vec4(vPos, 1.0);
}
