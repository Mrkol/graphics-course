#version 450
#extension GL_GOOGLE_include_directive : require

#include "gbuffer.glsl"

layout(set = 0, binding = 0) uniform sampler2D albedoRoughness;
layout(set = 0, binding = 1) uniform sampler2D normalMaterial;
layout(set = 0, binding = 2) uniform sampler2D depthMap;
layout(set = 0, binding = 3) uniform sampler2D decalAlbedo;
layout(set = 0, binding = 4) uniform sampler2D decalNormalRoughness;
layout(set = 0, binding = 5, std140) uniform Projectors {
    mat4 worldToLocal[3];
};

layout(push_constant) uniform PushConstants {
    mat4 inverseViewProjection;
    vec4 controls;
} pc;

layout(location = 0) out vec4 outAlbedoRoughness;
layout(location = 1) out vec4 outNormalMaterial;

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    outAlbedoRoughness = texelFetch(albedoRoughness, pixel, 0);
    outNormalMaterial = texelFetch(normalMaterial, pixel, 0);
    float depth = texelFetch(depthMap, pixel, 0).r;
    if (depth >= 1.0) return;

    vec3 surfaceNormal = DecodeNormal(outNormalMaterial.rg);
    float facing = smoothstep(0.2, 0.55, surfaceNormal.y);
    if (facing == 0.0) return;

    vec2 screenUv = gl_FragCoord.xy / vec2(textureSize(depthMap, 0));
    vec3 worldPosition = ReconstructPosition(screenUv, depth, pc.inverseViewProjection);
    vec3 normal = surfaceNormal;
    for (int i = 0; i < worldToLocal.length(); i++) {
        vec3 localPosition = (worldToLocal[i] * vec4(worldPosition, 1.0)).xyz;
        if (any(greaterThan(abs(localPosition), vec3(1.0)))) continue;

        vec2 decalUv = localPosition.xz * 0.5 + 0.5;
        vec4 color = textureLod(decalAlbedo, decalUv, 0.0);
        float depthFade = 1.0 - smoothstep(0.7, 1.0, abs(localPosition.y));
        float opacity = color.a * pc.controls.x * facing * depthFade;
        outAlbedoRoughness.rgb = mix(outAlbedoRoughness.rgb, color.rgb, opacity);

        float strength = opacity * pc.controls.y;
        if (strength == 0.0) continue;
        vec3 material = textureLod(decalNormalRoughness, decalUv, 0.0).rgb;
        outAlbedoRoughness.a = mix(outAlbedoRoughness.a, material.b, strength);

        vec3 projectorX = transpose(worldToLocal[i])[0].xyz;
        vec3 tangent = normalize(projectorX - surfaceNormal * dot(surfaceNormal, projectorX));
        vec3 bitangent = cross(tangent, surfaceNormal);
        vec2 normalXY = material.rg * 2.0 - 1.0;
        float normalZ = sqrt(max(1.0 - dot(normalXY, normalXY), 0.0));
        vec3 decalNormal = tangent * normalXY.x + bitangent * normalXY.y + surfaceNormal * normalZ;
        normal = normalize(mix(normal, decalNormal, strength));
        outNormalMaterial.rg = EncodeNormal(normal);
    }
}
