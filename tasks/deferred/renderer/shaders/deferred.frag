#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(binding = 0) uniform sampler2D albedo;
layout(binding = 1) uniform sampler2D normal;
layout(binding = 2) uniform sampler2D     wc;
layout(binding = 3) uniform sampler2D  depth;

layout(push_constant) uniform pc_t
{
    mat4 mProjView;
    vec4 position;
    vec4 color;
} params;

const vec2 resolution = vec2(1280, 720);

vec3 getPos(float depth, float wc) {
  return vec3(
    (2 * gl_FragCoord.x / resolution.x) - 1,
    (2 * gl_FragCoord.y / resolution.y) - 1,
    depth
  ) / wc;
}

vec4 getLight(vec3 lightPos, vec3 pos, vec3 normal, vec3 lightColor)
{
  const vec3 lightDir   = normalize(lightPos - pos);
  const vec3 diffuse = max(dot(normal, lightDir), 0.0f) * lightColor;
  const float ambient = 0.05;
  return vec4( (diffuse + ambient), 1.f);
}

void main(void)
{
  const vec2 texCoord = gl_FragCoord.xy / resolution;
  const vec3 surfaceColor = texture(albedo, texCoord).rgb;

  const vec4 normal_wc = texture(normal, texCoord);
  const mat3 ipv3 = transpose(inverse(mat3(params.mProjView)));
  const vec3 normal = normalize(ipv3 * normal_wc.xyz);
  const float depthV = texture(depth, texCoord).r;
  const float wc     = texture(wc,    texCoord).r;
  
  const vec3 pos = getPos(depthV, wc);
  // Only sunlight. Other are in sphere_deferred;
  out_fragColor = getLight((params.mProjView * vec4(params.position.xyz, 1)).xyz, pos, normal, params.color.rgb);
  out_fragColor.rgb *= surfaceColor;
}

