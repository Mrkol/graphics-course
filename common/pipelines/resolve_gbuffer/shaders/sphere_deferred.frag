#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec3 lightDir;
  vec4 lightSrc;
} surf;

layout(binding = 0) uniform sampler2D albedo;
layout(binding = 1) uniform sampler2D normal;
layout(binding = 2) uniform sampler2D material;
layout(binding = 3) uniform sampler2D wc;
layout(binding = 4) uniform sampler2D depth;

layout(push_constant) uniform pc_t
{
    mat4 mProj;
    mat4 mView;
    vec4 pos;
    vec4 color;
    float degree;
} params;
#include "pbr.glsl"

const vec2 resolution = vec2(1280, 720);

vec3 getPos(float depth, float wc) {
  return vec3(
    (2 * gl_FragCoord.x / resolution.x) - 1,
    (2 * gl_FragCoord.y / resolution.y) - 1,
    depth
  ) / wc;
}

vec4 getLight(vec3 pos, vec3 normal, vec3 lightColor, vec3 lightDir, vec3 surfaceColor, vec4 material)
{
  if(dot(normal, normalize(-lightDir)) < 0)
    return vec4(0);
  return vec4(lightColor * pbr_light(surfaceColor, pos, normal, normalize(-lightDir), material, vec3(1, 1, 1)), 1.f);
}

void main(void)
{
  out_fragColor = vec4(0, 0, 0, 0);
  vec2 texCoord = gl_FragCoord.xy / resolution;
  const vec3 surfaceColor = texture(albedo, texCoord).rgb;

  const vec4 normal_wc = texture(normal, texCoord);
  const mat3 ipv3 = transpose(inverse(mat3(params.mView)));

  vec3 normal = normal_wc.xyz;
  if(length(normal) < 0.5)
    normal = vec3(0, 1, 0);
  normal = normalize(ipv3 * normal);
  
  const float wc     = texture(wc,    texCoord).r;
  const float depthV = texture(depth, texCoord).w;
  const vec3 pos_screen = getPos(depthV, wc);
  const vec3 pos = inverse(mat3(params.mProj)) * pos_screen;
  
  const vec3 lightDir = pos - (params.mView * vec4(params.pos.xyz, 1)).xyz;
  const float dist = length(transpose(ipv3) * lightDir);
  if (dist < params.pos.w) {
    out_fragColor.rgb = getLight(pos, normal, params.color.rgb, lightDir, surfaceColor, texture(material, texCoord)).rgb;
    out_fragColor.a = max(sin(3.14 * (1 - dist / params.pos.w) / 2), 0);
  }
}
