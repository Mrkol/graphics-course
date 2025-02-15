#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec3 lightDir;
  vec4 lightSrc;
} surf;

layout(push_constant) uniform pc_t
{
    mat4 mProjView;
    mat4 mView;
    vec4 pos;
    vec4 color;
    float degree;
} params;


void main(void)
{
  out_fragColor = params.color;
  out_fragColor.a = 1;
}

