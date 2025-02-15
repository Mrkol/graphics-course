#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(binding = 0) uniform sampler2D image;

layout(binding = 1) readonly buffer d_t
{
  float p[128];
} distribution;



vec3 tonemap(vec3 color)
{
  float brightness = clamp(max(color.r, max(color.g, color.b)), 0., 1.);
  int bIdx = int(floor(127. * brightness));
  float p = distribution.p[bIdx];
  return p * color / brightness;
}

void main(void)
{
  vec4 color = texture(image, surf.texCoord);
  //out_fragColor = vec4(tonemap(color.rgb), 1);
  out_fragColor = vec4(color.rgb, 1);
}