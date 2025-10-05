#version 450

layout(set = 0, binding = 0) uniform sampler2D uScene;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
layout(push_constant) uniform PC { vec4 params; } pc;

#define INVRES (pc.params.xy)
#define TINT   (pc.params.z)
#define AAEN   (pc.params.w)

const float EDGE_THRESHOLD     = 0.022;
const float EDGE_THRESHOLD_MIN = 0.008;
const float SUBPIX_TRIM        = 0.75;
const float SUBPIX_CAP         = 0.55;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

vec3 bilinearFetch(sampler2D s, vec2 uv, vec2 invRes)
{
    ivec2 texSize = ivec2(round(1.0 / invRes));
    vec2  t = uv * vec2(texSize) - 0.5;
    ivec2 i = ivec2(floor(t));
    vec2  f = fract(t);
    i = clamp(i, ivec2(0), texSize - ivec2(2));
    vec3 c00 = texelFetch(s, i,               0).rgb;
    vec3 c10 = texelFetch(s, i + ivec2(1, 0), 0).rgb;
    vec3 c01 = texelFetch(s, i + ivec2(0, 1), 0).rgb;
    vec3 c11 = texelFetch(s, i + ivec2(1, 1), 0).rgb;
    return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
}

void main()
{
    vec2 px = INVRES;
    vec3  cM = texture(uScene, vUV).rgb;

    float lM = luma(cM);
    float lN = luma(texture(uScene, vUV + vec2(0.0, -px.y)).rgb);
    float lS = luma(texture(uScene, vUV + vec2(0.0,  px.y)).rgb);
    float lW = luma(texture(uScene, vUV + vec2(-px.x, 0.0)).rgb);
    float lE = luma(texture(uScene, vUV + vec2( px.x, 0.0)).rgb);
    float lMin = min(lM, min(min(lN, lS), min(lW, lE)));
    float lMax = max(lM, max(max(lN, lS), max(lW, lE)));
    float contrast = lMax - lMin;
    float lumaAvg  = (lM + lN + lS + lW + lE) * 0.2;
    float edgeMask = step(max(EDGE_THRESHOLD_MIN, EDGE_THRESHOLD * lumaAvg), contrast);

    if (AAEN < 0.5) {
        outColor = vec4(cM, 1.0);
        return;
    }

    if (edgeMask < 0.5) {
        outColor = vec4(cM, 1.0);
        return;
    }

    vec2 dir = vec2(-(lN + lS - 2.0*lM), (lW + lE - 2.0*lM));
    float dirReduce = max((lN + lS + lW + lE) * 0.125, 1e-4);
    float rcpMin    = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir             = clamp(dir * rcpMin * 0.5, -2.0*px, 2.0*px);

    vec3 cA = bilinearFetch(uScene, vUV - dir * (1.0/3.0), px);
    vec3 cB = bilinearFetch(uScene, vUV + dir * (1.0/3.0), px);
    vec3 cEdge = 0.5 * (cA + cB);

    float lEdge = luma(cEdge);
    float subpix = clamp((lEdge - lMin) * (lMax - lEdge) / (contrast*contrast + 1e-6), 0.0, 1.0);
    float blend  = clamp(SUBPIX_TRIM + subpix * (SUBPIX_CAP - SUBPIX_TRIM), 0.0, 1.0);

    vec3 outC = mix(cM, cEdge, blend);

    float lOut = luma(outC);
    if (lOut < lMin || lOut > lMax) outC = cM;

    outColor = vec4(outC, 1.0);

    if (TINT > 0.5 && AAEN > 0.5) {
        outColor.rgb = mix(outColor.rgb, vec3(0.65, 0.00, 0.85), 0.8 * edgeMask);
    }
}
