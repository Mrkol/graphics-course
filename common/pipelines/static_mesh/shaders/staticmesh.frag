#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 out_fragColor;
layout(location = 1) out vec4 out_fragNormal;
layout(location = 2) out vec4 out_fragMaterial;
layout(location = 3) out float out_fragWc;

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec4 wNorm;
  vec4 wTangent;
  vec2 texCoord;
  vec2 normTexCoord;
} surf;

layout (std140, set = 0, binding = 0) readonly buffer ims_t {
  mat4 mModels[]; 
} ims;
layout(set = 1, binding = 0) uniform sampler2D baseColorTexture;
layout(set = 1, binding = 1) uniform sampler2D    normalTexture;
layout(set = 1, binding = 2) uniform sampler2D      emr_Texture;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  mat4 mModel;
  vec4 color;
  vec4 emr_;
  uint relemIdx;
} params;


const vec2 resolution = vec2(1280, 720);

vec3 getPos() {
  return vec3(
    (2 * gl_FragCoord.x / resolution.x) - 1,
    (2 * gl_FragCoord.y / resolution.y) - 1,
    gl_FragCoord.z
  ) / gl_FragCoord.w;
}

void main()
{
  out_fragColor = texture(baseColorTexture, surf.texCoord) * params.color;
  vec3 bitangent = normalize(cross(surf.wNorm.xyz, surf.wTangent.xyz));
  vec4 normalMap = texture(normalTexture, surf.texCoord); 
  normalMap = 2 * normalMap - 1;
  out_fragNormal.xyz = normalize(surf.wNorm.xyz * normalMap.b + surf.wTangent.xyz * normalMap.g + bitangent * normalMap.r);
  //out_fragNormal = normalMap;
  out_fragWc = gl_FragCoord.w;
  out_fragMaterial = params.emr_ * texture(emr_Texture, surf.normTexCoord);
}
