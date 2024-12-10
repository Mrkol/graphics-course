#version 430
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D iChannel0;
layout(binding = 1) uniform sampler2D iChannel1;

layout(push_constant) uniform params
{
  uvec2 iResolution;
  uvec2 iMouse;
  float iTime;
};

float map(vec3 pos, out vec3 p) {
    float r1 = cos(iTime * 0.5) * 2.0;
    vec3 center1 = vec3(r1*cos(iTime*1.6), 0.0, r1*sin(iTime*1.6));
    p = pos - center1;
    float d = length(pos - center1) - 1.0;
    float r2 = cos(iTime * 0.75) * 4.0;
    vec3 center2 = vec3(r2*sin(iTime*1.7), r2*cos(iTime), 0.0*1.7);
    float d2 = length(pos - center2) - 1.0;
    if (d > d2) {
        d = d2;
        p = pos - center2;
    }
    float r3 = cos(iTime * 2.25) * 3.0;
    vec3 center3 = vec3(0.0, r3 * sin(iTime*2.0), r3 * cos(iTime*2.0));
    float d3 = length(pos - center3) - 1.0;
    if (d > d3) {
        d = d3;
        p = pos - center3;
    }
    return d;
}

vec3 getNormal(vec3 p) {
  vec2 d = vec2(0.01, 0.0);
  vec3 trash;
  float gx = map(p + d.xyy, trash) - map(p - d.xyy, trash);
  float gy = map(p + d.yxy, trash) - map(p - d.yxy, trash);
  float gz = map(p + d.yyx, trash) - map(p - d.yyx, trash);
  vec3 normal = vec3(gx, gy, gz);
  return normalize(normal);
}

vec3 triplanar(vec3 pos, sampler2D tex) {
    vec3 texCoordX = vec3(pos.y, pos.z, 0.0);
    vec3 texCoordY = vec3(pos.x, pos.z, 0.0);
    vec3 texCoordZ = vec3(pos.x, pos.y, 0.0);

    vec3 colorX = textureLod(tex, texCoordX.xy, 0).rgb;
    vec3 colorY = textureLod(tex, texCoordY.xy, 0).rgb;
    vec3 colorZ = textureLod(tex, texCoordZ.xy, 0).rgb;

    float wX = abs(pos.x);
    float wY = abs(pos.y);
    float wZ = abs(pos.z);

    float totalWeight = wX + wY + wZ;
    
    return (colorX * wX + colorY * wY + colorZ * wZ) / totalWeight;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    float I_a = 0.2;
    float I_d = 1.0;
    float I_s = 1.0;
    float n = 64.0;
    
    vec2 uv = fragCoord.xy / iResolution.xy;
    uv = uv * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;
    vec3 col = vec3(0.0);
    vec3 ro = vec3(0.0, 0.0, 6.0);
    vec3 rd = normalize(vec3(uv, -1.0));
    float t = 0.0;
    for (int i = 0; i < 64; i++)
    {
        vec3 p;
        vec3 pos = ro + rd * t;
        float d = map(pos, p);
        t += d;
        if (d < 0.001)
        {
            vec3 n = getNormal(pos);
            vec3 l = normalize(vec3(cos(iTime* 1.2), cos(iTime* 1.3) * sin(iTime* 1.4), sin(iTime * 1.5)));
            vec3 r = l - 2.0*dot(l, n) * n;
            float diff = clamp(I_d * dot(n, l), 0.0, 1.0);
            float spec = clamp(I_s * pow(max(0.0, dot(r, rd)), 64.0), 0.0, 1.0);
            float res = clamp(I_a + diff + spec, 0.0, 1.0);
            col = vec3(res);
            vec4 light = vec4(col, 1.0);
            vec4 texturing = vec4(triplanar(p, iChannel1), 1.0);
            fragColor = mix(light, texturing, 0.5);
            return;
        }
    }

    vec3 col2 = textureLod(iChannel0, rd.xy / 2 + 0.5, 0).rgb;
    
    fragColor = vec4(col2, 1.0);
}


void main()
{
  vec2 uv = vec2(gl_FragCoord).xy;
  // vec4 fragColor;

  mainImage(fragColor, uv);

  // TODO: Put your shadertoy code here!
  // Simple gradient as a test.
  // vec3 color = vec3(vec2(uv) / vec2(1280, 720), 0);

  // if (uv.x < 1280 && uv.y < 720)
  //   imageStore(resultImage, uv, fragColor);
}
