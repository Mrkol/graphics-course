#version 450
#extension GL_GOOGLE_include_directive : require

#include "terrain.glsl"

layout(vertices = 4) out;
layout(location = 0) patch out float tessellation;

float EdgeLevel(vec2 midpoint) {
    vec2 worldXZ = pc.chunk.xy + midpoint * pc.chunk.z;
    vec3 position = vec3(worldXZ.x, SampleHeight(worldXZ), worldXZ.y);
    float level = pc.terrain.w / max(distance(pc.cameraPosition.xyz, position), 1.0);

    return clamp(exp2(ceil(log2(max(level, 2.0)))), 2.0, pc.terrain.z);
}

void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = EdgeLevel(vec2(0.0, 0.5));
        gl_TessLevelOuter[1] = EdgeLevel(vec2(0.5, 0.0));
        gl_TessLevelOuter[2] = EdgeLevel(vec2(1.0, 0.5));
        gl_TessLevelOuter[3] = EdgeLevel(vec2(0.5, 1.0));
        gl_TessLevelInner[0] = max(gl_TessLevelOuter[1], gl_TessLevelOuter[3]);
        gl_TessLevelInner[1] = max(gl_TessLevelOuter[0], gl_TessLevelOuter[2]);
        tessellation = max(gl_TessLevelInner[0], gl_TessLevelInner[1]);
    }
}
