#version 450

void main(void) {
  gl_Position = vec4(-1.0 + 4.0 * float(gl_VertexIndex == 1), -1.0 + 4.0 * float(gl_VertexIndex == 2), 0.0, 1.0);
}