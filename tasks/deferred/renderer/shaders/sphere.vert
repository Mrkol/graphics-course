#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform pc_t
{
  mat4 mProjView;
  vec4 pos;
  vec4 color;
  float degree;
} params;

layout (location = 0 ) out VS_OUT {
  vec3 lightDir;
  vec4 lightSrc;
} vOut;

const float PI = 3.1415926535897932384626433832795;

void main() {
  int layer = gl_InstanceIndex + (gl_VertexIndex % 2);
  int step  = gl_VertexIndex / 2;
  const float xy_r = max(sin(params.degree * layer), 0.001);
  const vec3 rad = vec3(cos(2 * params.degree * step) * xy_r, sin(2 * params.degree * step) * xy_r, cos(params.degree * layer));
  gl_Position = (params.mProjView * vec4(params.pos.xyz + params.pos.w * rad, 1));
  
  //const mat3 ipv3 = transpose(inverse(mat3(params.mProjView)));
  vOut.lightSrc.xyz = (params.mProjView * vec4(params.pos.xyz, 1)).xyz;
 
  vOut.lightDir = normalize(gl_Position.xyz - vOut.lightSrc.xyz);
  vOut.lightSrc.w = length(gl_Position.xyz - vOut.lightSrc.xyz);
}
