#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(push_constant) uniform params
{
  float iTime;
} pushConstant;

layout(binding = 0) uniform sampler2D texStars;

#define PI 3.14159265359

// Function to generate a smooth, repeating gradient
vec3 gradient(vec2 uv) {
    float angle = atan(uv.y, uv.x) + pushConstant.iTime;
    float dist = length(uv);
    return vec3(sin(angle * 10.0 + dist * 5.0), sin(angle * 20.0 + dist * 10.0), sin(angle * 30.0 + dist * 15.0));
}

// Function to generate a rotating spiral pattern
vec3 spiral(vec2 uv) {
    float angle = atan(uv.y, uv.x) + pushConstant.iTime * 2.0;
    float radius = length(uv) * 5.0;
    float value = sin(angle * 10.0 + radius);
    return vec3(value, value * 0.5, value * 0.25);
}

void main() {
    // Choose between different patterns using a simple condition
    vec3 color;
    // if (surf.texCoord.x < 0.5) {
        // color = gradient(surf.texCoord * 2.0);
    // } else {
        color = spiral(surf.texCoord * 2.0);
    // }

    fragColor = vec4(color, 1.0);

    // if (fragCoord.x < 1280 && fragCoord.y < 720) {
    //   imageStore(resultImage, fragCoord, fragColor);
    // }
}
