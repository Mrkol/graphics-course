#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform params
{
  float iTime;
} pushConstant;

layout (location = 0 ) out VS_OUT
{
  vec2 texCoord;
} vOut;

void main() {
  vec2 xy = gl_VertexIndex == 0 ? vec2(-1, -1) : (gl_VertexIndex == 1 ? vec2(3, -1) : vec2(-1, 3));
  gl_Position = vec4(xy, 0, 1);
  vOut.texCoord = xy * 0.5 + 0.5;
}