#ifndef LIGHT_PASS_FRAG_IMPL_H
#define LIGHT_PASS_FRAG_IMPL_H

#ifndef LIGHT_PARAMS_TYPE
#error "LIGHT_PARAMS_TYPE is not defined"
#endif

#ifndef LIGHT_GET_COLOR
#error "LIGHT_GET_COLOR is not defined"
#endif


layout(location = 0) out vec4 out_fragColor;

layout(location = 0) in VS_OUT
{
  noperspective vec2 texCoord;
  LIGHT_PARAMS_TYPE light;
}
fIn;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
}
params;

layout(binding = 1, set = 0) uniform sampler2D baseColorTexture;
layout(binding = 2, set = 0) uniform sampler2D normalTexture;
layout(binding = 3, set = 0) uniform sampler2D emissiveTexture;
layout(binding = 4, set = 0) uniform sampler2D occlusionMetallicRoughnessTexture;
layout(binding = 5, set = 0) uniform sampler2D depthTexture;

void main()
{
  SurfacePointInfo info;

  info.baseColor = texture(baseColorTexture, fIn.texCoord).rgb;
  info.normal = texture(normalTexture, fIn.texCoord).xyz * 2.0 - 1.0;
  info.emissive = texture(emissiveTexture, fIn.texCoord).rgb;

  vec3 occlusionMetallicRoughness = texture(occlusionMetallicRoughnessTexture, fIn.texCoord).rgb;
  info.occlusion = occlusionMetallicRoughness.r;
  info.metallic = occlusionMetallicRoughness.g;
  info.roughness = occlusionMetallicRoughness.b;

  float depth = texture(depthTexture, fIn.texCoord).r;

  vec4 positionH = inverse(params.mProjView) * vec4(vec3(fIn.texCoord * 2.0 - 1.0, depth), 1.0);
  vec3 position = positionH.xyz / positionH.w;

  info.position = position;

  vec4 cameraPositionH = inverse(params.mProjView) * vec4(vec3(0.0), 1.0);
  vec3 cameraPosition = cameraPositionH.xyz / cameraPositionH.w;

  info.cameraVec = cameraPosition - position;

  out_fragColor = vec4(LIGHT_GET_COLOR(fIn.light, info), 1.0);
}

#endif
