#version 430
#extension GL_GOOGLE_include_directive : require


void main(void)
{
  if (gl_VertexIndex == 0) {
    gl_Position = vec4(-1.0f, -1.0f, 0.0f, 1.0f);
  } else if (gl_VertexIndex == 1) {
    gl_Position = vec4(3.0f, -1.0f, 0.0f, 1.0f);
  } else {
    gl_Position = vec4(-1.0f, 3.0f, 0.0f, 1.0f);
  }
}