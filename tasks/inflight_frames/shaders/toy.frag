#version 430
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D iChannel0;

layout(binding = 1) uniform sampler2D iChannel1;

layout(push_constant) uniform PushConstants
{
  float iTime;
  vec2 iResolution;
}
params;

float sphereSDF(vec3 p, float r) {
    return length(p) - r;
}

vec3 calcNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        sphereSDF(p + vec3(e.x, e.y, 0.0), 1.0) - sphereSDF(p - vec3(e.x, e.y, 0.0), 1.0),
        sphereSDF(p + vec3(0.0, e.x, e.y), 1.0) - sphereSDF(p - vec3(0.0, e.x, e.y), 1.0),
        sphereSDF(p + vec3(e.y, 0.0, e.x), 1.0) - sphereSDF(p - vec3(e.y, 0.0, e.x), 1.0)
    ));
}

float rayMarch(vec3 ro, vec3 rd, out vec3 hitPoint, out vec3 normal) {
    float dist = 0.0;
    for (int i = 0; i < 100; i++) {
        hitPoint = ro + rd * dist;
        float d = sphereSDF(hitPoint, 1.0);
        if (d < 0.01) {
            normal = calcNormal(hitPoint);
            return dist;
        }
        dist += d;
    }
    return 100.0;
}

vec3 phongLighting(vec3 p, vec3 normal, vec3 lightPos, vec3 viewDir) {
    vec3 lightDir = normalize(lightPos - p);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    return vec3(0.1) + vec3(0.8) * diff + vec3(1.0) * spec;
}

vec3 triplanarTexture(vec3 p, vec3 normal) {
    vec3 absNormal = abs(normal);
    absNormal /= (absNormal.x + absNormal.y + absNormal.z);

    vec3 xProjection = texture(iChannel1, p.yz * 0.5).rgb * absNormal.x;
    vec3 yProjection = texture(iChannel1, p.xz * 0.5).rgb * absNormal.y;
    vec3 zProjection = texture(iChannel1, p.xy * 0.5).rgb * absNormal.z;

    return xProjection + yProjection + zProjection;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - params.iResolution.xy * 0.5) / params.iResolution.y;

    vec3 ro = vec3(0.0, 0.0, 3.0);
    vec3 rd = normalize(vec3(uv, -1.0));

    vec3 hitPoint, normal;
    float dist = rayMarch(ro, rd, hitPoint, normal);

    if (dist > 99.9) {
    return;
    }

    vec3 lightPos = vec3(2.0 * sin(params.iTime), 2.0 * sin(params.iTime), 2.0 * cos(params.iTime));
    vec3 viewDir = -rd;

    vec3 textureColor = triplanarTexture(hitPoint, normal);
    vec3 lighting = phongLighting(hitPoint, normal, lightPos, viewDir);
    vec3 color = lighting * textureColor;

    fragColor = vec4(color, 1.0);
}

void main()
{
  mainImage(color, gl_FragCoord.xy);
}
