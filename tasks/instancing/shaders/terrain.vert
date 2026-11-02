#version 450

layout(set = 0, binding = 0) uniform sampler2D heightMap;

layout(push_constant) uniform DrawSettings {
    mat4 viewProj;
    vec4 light;
    vec4 eye;
    vec4 detail;
    vec4 levels;
} params;

layout(location = 0) out vec2 oUV;
layout(location = 1) out vec3 oWorld;

void main() {

    int x = gl_VertexIndex % 257;
    int y = gl_VertexIndex / 257;
    vec2 uv = vec2(x, y) / 256.0;
    float elevation = textureLod(heightMap, uv, 0.0).r;

    vec3 pos = vec3((uv.x - 0.5) * 100.0, max(elevation, 0.0),
                    (uv.y - 0.5) * 100.0);
    oUV = uv;
    oWorld = pos;
    gl_Position = params.viewProj * vec4(pos, 1.0);
}
