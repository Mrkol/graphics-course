#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


layout(location = 0) out vec4 out_fragColor;

layout(location = 0) in VS_OUT
{
  noperspective vec2 texCoord;
}
fIn;

layout(binding = 0, set = 0) uniform sampler2D emissiveTexture;

void main()
{
  out_fragColor = vec4(texture(emissiveTexture, fIn.texCoord).rgb, 1.0);
}
