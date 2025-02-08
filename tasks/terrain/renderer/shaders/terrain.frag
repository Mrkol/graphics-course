#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 out_fragColor;

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
  vec4 normal;
  float height;
} surf;

layout(binding = 0) uniform sampler2D hmap;

void main(void)
{
    out_fragColor = fract(surf.texCoord.x * 64.) < 0.5 ? vec4(1., 0., 0., 0) : vec4(0., 1., 0., 0);
    out_fragColor *= 0.01 + pow(max(0., dot(surf.normal, normalize(vec4(.0, -1., 0., 0)))), 4);
    //out_fragColor = vec4(0.01);
    //out_fragColor += vec4(surf.normal.xyz + vec3(1., 1., 1.), 1) * 0.5;
    //out_fragColor *= fract(surf.texCoord.x * 64.) < 0.5 ? 1. : 0.2;

}