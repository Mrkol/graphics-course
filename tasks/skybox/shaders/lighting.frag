#version 450
#extension GL_GOOGLE_include_directive : require

#include "gbuffer.glsl"

layout(set = 0, binding = 0) uniform sampler2D albedoRoughness;
layout(set = 0, binding = 1) uniform sampler2D normalMetallicAo;
layout(set = 0, binding = 2) uniform sampler2D depthBuffer;

layout(set = 0, binding = 3, std140) uniform Lighting {
    mat4 inverseViewProjection;
    vec4 cameraPosition;
    vec4 sunDirectionIntensity;
    vec4 sunColor;
    mat4 inverseSkyViewProjection;
} lighting;

layout(set = 0, binding = 4) uniform samplerCube skyCubemap;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

float Geometry(float cosine, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return cosine / max(cosine * (1.0 - k) + k, 0.0001);
}

vec3 EvaluateLight(vec3 n, vec3 v, vec3 l, vec3 albedo, float roughness, float metallic) {
    float nl = max(dot(n, l), 0.0);
    if (nl == 0.0) return vec3(0.0);

    vec3 halfVector = v + l;
    vec3 h = halfVector * inversesqrt(max(dot(halfVector, halfVector), 0.0001));
    float nv = max(dot(n, v), 0.0001);
    float nh = max(dot(n, h), 0.0);
    float a = roughness * roughness;
    float denominator = nh * nh * (a * a - 1.0) + 1.0;
    float distribution = a * a / max(PI * denominator * denominator, 0.0001);

    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - clamp(dot(h, v), 0.0, 1.0), 5.0);
    vec3 specular = distribution * Geometry(nv, roughness) * Geometry(nl, roughness) * fresnel / max(4.0 * nv * nl, 0.0001);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * albedo / PI;
    return (diffuse + specular) * nl;
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    float depth = texelFetch(depthBuffer, pixel, 0).r;
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(depthBuffer, 0));
    if (depth == 1.0) {
        vec3 rayDir = normalize(ReconstructPosition(uv, 1.0, lighting.inverseSkyViewProjection));
        vec3 sky = textureLod(skyCubemap, rayDir, 0.0).rgb;
        outColor = vec4(pow(max(sky, vec3(0.0)), vec3(1.0 / 2.2)), 1.0);
        return;
    }

    vec3 position = ReconstructPosition(uv, depth, lighting.inverseViewProjection);
    vec4 albedoData = texelFetch(albedoRoughness, pixel, 0);
    vec4 normalData = texelFetch(normalMetallicAo, pixel, 0);
    vec3 albedo = albedoData.rgb;
    float roughness = clamp(albedoData.a, 0.08, 1.0);
    float metallic = normalData.b;
    vec3 n = DecodeNormal(normalData.rg);
    vec3 toCamera = lighting.cameraPosition.xyz - position;
    float cameraDistance = length(toCamera);
    vec3 v = toCamera / max(cameraDistance, 0.0001);

    vec3 color = albedo * vec3(0.16, 0.2, 0.25) * normalData.a;
    vec3 sunDirection = lighting.sunDirectionIntensity.xyz;
    vec3 sunRadiance = lighting.sunColor.rgb * lighting.sunDirectionIntensity.w;
    color += EvaluateLight(n, v, sunDirection, albedo, roughness, metallic) * sunRadiance;

    float fog = 1.0 - exp(-cameraDistance * 0.00065);
    color = mix(color, vec3(0.48, 0.67, 0.85), fog);
    outColor = vec4(pow(max(color, vec3(0.0)), vec3(1.0 / 2.2)), 1.0);
}
