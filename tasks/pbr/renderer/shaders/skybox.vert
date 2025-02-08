#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0 ) out VS_OUT
{
  vec3 texCoord;
} vOut;

layout(push_constant) uniform pc {
    mat4 mProjView;
} params;


void main() {
  vec2 xy = gl_VertexIndex == 0 ? vec2(-1, -1) : (gl_VertexIndex == 1 ? vec2(3, -1) : vec2(-1, 3));
  gl_Position = vec4(xy, 1, 1);
  vOut.texCoord = inverse(mat3(params.mProjView)) * vec3(xy.x, xy.y, 1);
}

