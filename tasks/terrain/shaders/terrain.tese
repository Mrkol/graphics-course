#version 450
#extension GL_GOOGLE_include_directive : require

#include "terrain.glsl"

layout(quads, equal_spacing, ccw) in;
layout(location = 0) patch in float tessellation;
layout(location = 0) out vec3 worldPosition;
layout(location = 1) out float lod;

invariant gl_Position;

void main() {
    precise vec2 worldXZ = pc.chunk.xy + gl_TessCoord.xy * pc.chunk.z;
    worldPosition = vec3(worldXZ.x, SampleHeight(worldXZ), worldXZ.y);
    lod = tessellation;
    gl_Position = pc.viewProjection * vec4(worldPosition, 1.0);
}
