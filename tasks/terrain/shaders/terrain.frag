#version 450
#extension GL_GOOGLE_include_directive : require

#include "terrain.glsl"

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in float lod;
layout(location = 0) out vec4 outColor;

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
    vec3 albedo = mix(rock, grass, smoothstep(0.55, 0.9, normal.y));
    albedo = mix(albedo, snow, smoothstep(105.0, 135.0, worldPosition.y) * smoothstep(0.6, 0.95, normal.y));

    if (pc.chunk.w > 0.5) {
        float level = clamp(log2(lod) / log2(pc.terrain.z), 0.0, 1.0);
        albedo = mix(vec3(0.04, 0.2, 0.8), vec3(0.95, 0.15, 0.03), level);
    }

    vec3 lightDirection = normalize(vec3(-0.5, 0.8, -0.3));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 color = albedo * (vec3(0.16, 0.2, 0.25) + vec3(1.0, 0.92, 0.8) * diffuse);
    float fog = 1.0 - exp(-distance(pc.cameraPosition.xyz, worldPosition) * 0.00065);
    color = mix(color, vec3(0.48, 0.67, 0.85), fog);
    outColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
