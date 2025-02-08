#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0 ) out VS_OUT
{
  vec2 texCoord;
} vOut;

layout(push_constant) uniform params_t
{
  vec2 base; 
  vec2 extent;
  mat4 mProjView;
  vec3 camPos;
  int degree;
} params;

void main() {

  vec2 xy = vec2(0, 0);
  if (gl_VertexIndex == 0) 
    xy = vec2( 0,  0);
  if (gl_VertexIndex == 1) 
    xy = vec2( 0,  1);
  if (gl_VertexIndex == 2) 
    xy = vec2( 1,  0);
  if (gl_VertexIndex == 3) 
    xy = vec2( 1,  1);
  xy = params.base + xy * params.extent;
  gl_Position = vec4(20 * xy.x, -7, 20 * xy.y, 1);
  vOut.texCoord = xy * 0.5 + 0.5;
}
