#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

const vec2 vertices[] = vec2[](vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));

const uint indices[] = uint[](0, 1, 3, 0, 3, 2);

layout(location = 0) out VS_OUT
{
  noperspective vec2 texCoord;
}
vOut;

void main()
{
  vec4 position = vec4(vertices[indices[gl_VertexIndex]], 0.0, 1.0);

  vOut.texCoord = (position.xy + 1.0) / 2.0;

  gl_Position = position;
}
