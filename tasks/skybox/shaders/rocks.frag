#version 450
#extension GL_GOOGLE_include_directive : require

#include "rocks.glsl"
#include "gbuffer.glsl"

layout(early_fragment_tests) in;

layout(set = 0, binding = 1) uniform sampler2D albedoMap;
layout(set = 0, binding = 2) uniform sampler2D normalMap;
layout(set = 0, binding = 3) uniform sampler2D armMap;
layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 tint;
layout(location = 0) out vec4 outAlbedoRoughness;
layout(location = 1) out vec4 outNormalMetallicAo;

vec3 SurfaceNormal() {
    vec3 n = normalize(worldNormal);
    vec3 dx = dFdx(worldPosition);
    vec3 dy = dFdy(worldPosition);
    vec2 du = dFdx(texCoord);
    vec2 dv = dFdy(texCoord);
    float determinant = du.x * dv.y - du.y * dv.x;
    if (abs(determinant) < 1e-10) return n;
    vec3 tangent = (dx * dv.y - dy * du.y) / determinant;
    vec3 bitangent = (-dx * dv.x + dy * du.x) / determinant;
    tangent = normalize(tangent - n * dot(n, tangent));
    float orientation = dot(cross(n, tangent), bitangent) < 0.0 ? -1.0 : 1.0;
    bitangent = cross(n, tangent) * orientation;
    vec3 mapped = texture(normalMap, texCoord).xyz * 2.0 - 1.0;
    mapped.xy *= pc.material.z;
    return normalize(mat3(tangent, bitangent, n) * mapped);
}

void main() {
    vec3 albedo = clamp(texture(albedoMap, texCoord).rgb * tint, 0.0, 1.0);
    vec3 arm = texture(armMap, texCoord).rgb;

    float roughness = clamp(arm.g * pc.material.x, 0.08, 1.0);
    float metallic = clamp(arm.b * pc.material.y, 0.0, 1.0);

    outAlbedoRoughness = vec4(albedo, roughness);
    outNormalMetallicAo = vec4(EncodeNormal(SurfaceNormal()), metallic, arm.r);
}
