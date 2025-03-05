#version 430
#extension GL_GOOGLE_include_directive : require

void main() {
    vec2 xy = gl_VertexIndex != 0 ? (gl_VertexIndex != 1 ? vec2(-1, 3) : vec2(3, -1)) : vec2(-1, -1);
    gl_Position = vec4(xy, 0, 1);
}