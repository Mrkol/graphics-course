#version 450

layout(set = 0, binding = 0) uniform sampler2D heightMap;
layout(set = 0, binding = 1) uniform sampler2D albedoAtlas;
layout(set = 0, binding = 2) uniform sampler2D roughnessAtlas;
layout(set = 0, binding = 3) uniform sampler2D detailHeightAtlas;

layout(push_constant) uniform DrawSettings {
    mat4 viewProj;
    vec4 light;
    vec4 eye;
    vec4 detail;
    vec4 levels;
} params;

layout(location = 0) in vec2 iUV;
layout(location = 1) in vec3 iWorld;
layout(location = 0) out vec4 outColor;

const float PI = 3.141592653589793;

vec2 materialUV(int m, vec2 localUV) {
    vec2 texelSafe = clamp(fract(localUV), vec2(1.0/256.0), vec2(255.0/256.0));
    return vec2((float(m) + texelSafe.x) * 0.25, texelSafe.y);
}

struct Surface { vec3 albedo; float roughness; vec2 slope; };
Surface material(int m, vec2 uv) {
    vec2 localUV = uv * params.detail.x;
    vec2 a = materialUV(m, localUV);
    Surface r;
    r.albedo = texture(albedoAtlas, a).rgb;
    r.roughness = texture(roughnessAtlas, a).r;

    vec2 stepUV = vec2(1.0 / 256.0);
    float dhdx = texture(detailHeightAtlas, materialUV(m, localUV + vec2(stepUV.x, 0))).r
                - texture(detailHeightAtlas, materialUV(m, localUV - vec2(stepUV.x, 0))).r;
    float dhdy = texture(detailHeightAtlas, materialUV(m, localUV + vec2(0, stepUV.y))).r
                - texture(detailHeightAtlas, materialUV(m, localUV - vec2(0, stepUV.y))).r;
    r.slope = vec2(dhdx, dhdy);
    return r;
}

float ggxDistribution(float NoH, float roughness) {
    float a = roughness * roughness;
    float a2 = a*a;
    float d = NoH*NoH*(a2-1.0)+1.0;
    return a2 / max(PI*d*d, 0.00001);
}
float smithG1(float NoX, float k) {
    return NoX / max(NoX*(1.0-k)+k, 0.00001);
}
vec3 fresnelSchlick(float VoH, vec3 f0) {
    return f0 + (1.0-f0)*pow(1.0-VoH, 5.0);
}

void main() {
    const float pixelWorldStep = 100.0 / 1024.0;
    const vec2 stepUV = vec2(1.0 / 1024.0);

    float hL = max(textureLod(heightMap, clamp(iUV - vec2(stepUV.x,0), 0.0, 1.0), 0).r, 0.0);
    float hR = max(textureLod(heightMap, clamp(iUV + vec2(stepUV.x,0), 0.0, 1.0), 0).r, 0.0);
    float hD = max(textureLod(heightMap, clamp(iUV - vec2(0,stepUV.y), 0.0, 1.0), 0).r, 0.0);
    float hU = max(textureLod(heightMap, clamp(iUV + vec2(0,stepUV.y), 0.0, 1.0), 0).r, 0.0);
    vec2 geomSlope = vec2((hR - hL), (hU - hD)) / (2.0 * pixelWorldStep);

    float h = textureLod(heightMap, iUV, 0).r;

    if (h < 0.0) {
        float depth = smoothstep(0.03, 0.63, -h);
        vec3 water = mix(vec3(0.08, 0.40, 0.46), vec3(0.025, 0.13, 0.23), depth);
        vec3 V = normalize(params.eye.xyz - iWorld);
        vec3 L = normalize(params.light.xyz - iWorld);
        float highlight = pow(max(dot(reflect(-L, vec3(0.0, 1.0, 0.0)), V), 0.0), 80.0);
        water += vec3(0.25) * highlight * params.light.w * 0.2;
        outColor = vec4(pow(water, vec3(1.0 / 2.2)), 1.0);
        return;
    }
    float t01 = smoothstep(params.levels.x - 1.5, params.levels.x + 1.5, h);
    float t12 = smoothstep(params.levels.y - 2.0, params.levels.y + 2.0, h);
    float t23 = smoothstep(params.levels.z - 2.5, params.levels.z + 2.5, h);
    float w0 = 1.0 - t01;
    float w1 = t01 * (1.0-t12);
    float w2 = t01 * t12 * (1.0-t23);
    float w3 = t01 * t12 * t23;
    Surface sand = material(0, iUV);
    Surface grass = material(1, iUV);
    Surface rock = material(2, iUV);
    Surface snow = material(3, iUV);
    vec3 albedo = sand.albedo*w0 + grass.albedo*w1 + rock.albedo*w2 + snow.albedo*w3;
    float roughness = clamp(sand.roughness*w0 + grass.roughness*w1 +
                            rock.roughness*w2 + snow.roughness*w3 + params.detail.z, 0.08, 1.0);
    vec2 bump = sand.slope*w0 + grass.slope*w1 + rock.slope*w2 + snow.slope*w3;
    vec3 N = normalize(vec3(-(geomSlope.x + bump.x * params.detail.y),
                             1.0,
                            -(geomSlope.y + bump.y * params.detail.y)));

    vec3 V = normalize(params.eye.xyz - iWorld);
    vec3 L = normalize(params.light.xyz - iWorld);
    vec3 H = normalize(V+L);
    float NoL = max(dot(N,L), 0.0);
    float NoV = max(dot(N,V), 0.0001);
    float NoH = max(dot(N,H), 0.0);
    float VoH = max(dot(V,H), 0.0);

    vec3 F0 = vec3(0.04);
    vec3 F = fresnelSchlick(VoH, F0);
    float D = ggxDistribution(NoH, roughness);
    float k = pow(roughness + 1.0, 2.0) / 8.0;
    float G = smithG1(NoL, k) * smithG1(NoV, k);
    vec3 specular = (D*G*F) / max(4.0*NoL*NoV, 0.001);
    vec3 diffuse = (vec3(1.0)-F) * albedo / PI;
    float distanceToLight = length(params.light.xyz - iWorld);
    float attenuation = 1.0 / (1.0 + 0.0015 * distanceToLight*distanceToLight);
    vec3 color = (diffuse + specular) * NoL * params.light.w * attenuation;
    color += albedo * params.detail.w;

    color *= params.levels.w;
    color = color / (color + vec3(1.0));
    color = pow(max(color, vec3(0.0)), vec3(1.0/2.2));
    outColor = vec4(color, 1.0);
}
