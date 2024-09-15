#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require


layout(location = 0) out vec4 out_baseColor;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_emissive;
layout(location = 3) out vec4 out_occlusionMetallicRoughness;

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} surf;

layout(push_constant) uniform params_t {
  mat4 mProjView;
  mat4 mModel;
  vec4 baseColorMetallicFactor;
  vec4 emissiveRoughnessFactors;
} params;

layout(binding = 0, set = 0) uniform sampler2D baseColorTexture;
layout(binding = 1, set = 0) uniform sampler2D metallicRoughnessTexture;
layout(binding = 2, set = 0) uniform sampler2D normalTexture;
layout(binding = 3, set = 0) uniform sampler2D occlusionTexture;
layout(binding = 4, set = 0) uniform sampler2D emissiveTexture;

void main()
{
  vec3 baseColor = texture(baseColorTexture, surf.texCoord).rgb * params.baseColorMetallicFactor.rgb;

  out_baseColor = vec4(baseColor, 1.0);

  vec3 normal = normalize(surf.wNorm);
  vec3 tangent = normalize(surf.wTangent - dot(surf.wTangent, normal) * normal);
  vec3 bitangent = cross(normal, tangent);

  vec3 actualNormalLocal = texture(normalTexture, surf.texCoord).xyz * 2.0 - 1.0;
  vec3 actualNormal = mat3(tangent, bitangent, normal) * actualNormalLocal;

  out_normal = vec4((actualNormal + 1.0) / 2.0, 1.0);

  vec3 emissive = texture(emissiveTexture, surf.texCoord).rgb *
    params.emissiveRoughnessFactors.rgb;

  out_emissive = vec4(emissive, 1.0);

  float occlusion = texture(occlusionTexture, surf.texCoord).r;
  vec2 metallicRoughness = texture(metallicRoughnessTexture, surf.texCoord).bg * vec2(params.baseColorMetallicFactor.a, params.emissiveRoughnessFactors.a);

  out_occlusionMetallicRoughness = vec4(occlusion, metallicRoughness, 1.0);
}
