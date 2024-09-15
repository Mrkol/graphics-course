#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) out vec4 out_fragColor;

layout(location = 0) in VS_OUT
{
  vec3 color;
} fIn;

void main()
{
  out_fragColor = vec4(fIn.color, 1.0);
}
