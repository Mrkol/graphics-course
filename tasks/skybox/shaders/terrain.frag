#version 450
#extension GL_GOOGLE_include_directive : require

#include "terrain.glsl"
#include "gbuffer.glsl"

layout(early_fragment_tests) in;

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in float lod;
layout(location = 0) out vec4 outAlbedoRoughness;
layout(location = 1) out vec4 outNormalMetallicAo;

void main() {
    float stepSize = pc.terrain.x / float(textureSize(heightmap, 0).x - 1);
    vec2 position = worldPosition.xz;
    float left = SampleHeight(position - vec2(stepSize, 0.0));
    float right = SampleHeight(position + vec2(stepSize, 0.0));
    float down = SampleHeight(position - vec2(0.0, stepSize));
    float up = SampleHeight(position + vec2(0.0, stepSize));
    vec3 normal = normalize(vec3(left - right, 2.0 * stepSize, down - up));

    vec3 grass = vec3(0.16, 0.29, 0.075);
    vec3 rock = vec3(0.32, 0.28, 0.23);
    vec3 snow = vec3(0.82, 0.86, 0.87);
    float grassWeight = smoothstep(0.55, 0.9, normal.y);
    float snowWeight = smoothstep(105.0, 135.0, worldPosition.y) * smoothstep(0.6, 0.95, normal.y);
    vec3 albedo = mix(mix(rock, grass, grassWeight), snow, snowWeight);
    float roughness = mix(mix(0.82, 0.94, grassWeight), 0.65, snowWeight);

    if (pc.chunk.w > 0.5) {
        float level = clamp(log2(lod) / log2(pc.terrain.z), 0.0, 1.0);
        albedo = mix(vec3(0.04, 0.2, 0.8), vec3(0.95, 0.15, 0.03), level);
    }

    outAlbedoRoughness = vec4(albedo, roughness);
    outNormalMetallicAo = vec4(EncodeNormal(normal), 0.0, 1.0);
}
