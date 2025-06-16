#version 450

void main()
{
  vec2 xy = gl_VertexIndex == 0 ? vec2(-1, -1) : (gl_VertexIndex == 1 ? vec2(3, -1) : vec2(-1, 3));
  gl_Position = vec4(xy, 0, 1);
}
