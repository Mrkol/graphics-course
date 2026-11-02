#version 450

layout(push_constant) uniform DrawSettings {
    mat4 viewProj;
    vec4 light;
    vec4 eye;
    vec4 detail;
    vec4 levels;
} params;

layout(location = 0) in vec3 iWorld;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec4 iColor;
layout(location = 0) out vec4 outColor;

const float PI = 3.141592653589793;

float hash(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float noise3(vec3 p) {
    vec3 b = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(mix(hash(b), hash(b + vec3(1,0,0)), f.x),
            mix(hash(b + vec3(0,1,0)), hash(b + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash(b + vec3(0,0,1)), hash(b + vec3(1,0,1)), f.x),
            mix(hash(b + vec3(0,1,1)), hash(b + vec3(1,1,1)), f.x), f.y), f.z);
}

float distributionGGX(float NoH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 0.00001);
}

float smithG1(float NoX, float k) {
    return NoX / max(NoX * (1.0 - k) + k, 0.00001);
}

vec3 fresnelSchlick(float VoH, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - VoH, 5.0);
}

void main() {
    vec3 N = normalize(iNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(params.eye.xyz - iWorld);
    vec3 L = normalize(params.light.xyz - iWorld);
    vec3 H = normalize(V + L);
    float stone = noise3(iWorld * 2.7) * 0.65 + noise3(iWorld * 11.0) * 0.35;
    float mineral = smoothstep(0.57, 0.78, noise3(iWorld * 1.5));
    vec3 albedo = iColor.rgb * mix(0.72, 1.23, stone);
    albedo = mix(albedo, albedo * vec3(1.12, 0.94, 0.77), mineral * 0.20);
    float roughness = clamp(iColor.a + 0.15 * (stone - 0.5), 0.12, 1.0);
    float NoL = max(dot(N, L), 0.0);
    float NoV = max(dot(N, V), 0.0001);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);
    vec3 F = fresnelSchlick(VoH, vec3(0.04));
    float D = distributionGGX(NoH, roughness);
    float k = pow(roughness + 1.0, 2.0) / 8.0;
    float G = smithG1(NoL, k) * smithG1(NoV, k);
    vec3 specular = (D * G * F) / max(4.0 * NoL * NoV, 0.001);
    vec3 diffuse = (1.0 - F) * albedo / PI;
    float lightDistance = length(params.light.xyz - iWorld);
    float attenuation = 1.0 / (1.0 + 0.0015 * lightDistance * lightDistance);
    vec3 color = (diffuse + specular) * NoL * params.light.w * attenuation;
    color += albedo * params.detail.w;
    color *= params.levels.w;
    color = color / (color + vec3(1.0));
    outColor = vec4(pow(max(color, vec3(0.0)), vec3(1.0 / 2.2)), 1.0);
}
