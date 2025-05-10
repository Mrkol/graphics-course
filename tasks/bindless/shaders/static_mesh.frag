#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require


layout(location = 0) out vec4 out_fragColor;

layout(location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
  flat int drawID;
} surf;

layout(push_constant) uniform params
{
  mat4 mProjView;
} pushConstant;

struct Draw {
  mat4 mModel;
  int relemIdx;
  int padding[1];
};

layout(set=0, binding=0) uniform sampler2D albedo[32];
layout(set=1, binding=0) readonly buffer RelemToTex {
  int texIndex[];
};
layout(set=2, binding=0) readonly buffer Draws {
  Draw draws[];
};

void main()
{
  const vec3 wLightPos = vec3(10, 10, 10);
  int texIdx = texIndex[nonuniformEXT(draws[surf.drawID].relemIdx)];
  const vec3 surfaceColor = texture(albedo[nonuniformEXT(texIdx)], surf.texCoord).xyz;

  const vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);

  const vec3 lightDir   = normalize(wLightPos - surf.wPos);
  const vec3 diffuse = max(dot(surf.wNorm, lightDir), 0.0f) * lightColor;
  const float ambient = 0.05;
  out_fragColor.rgb = (diffuse + ambient) * surfaceColor;
  out_fragColor.a = 1.0f;
}
