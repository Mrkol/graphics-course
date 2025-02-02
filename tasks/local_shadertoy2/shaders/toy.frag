#version 430


layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D emp;
layout(binding = 1) uniform sampler2D iChannel0;
layout(push_constant) uniform params {
  uint resolution_x;
  uint resolution_y;
  float time;
} pushed_params;

float iTime;
vec2 iResolution;
vec2 iMouse;
vec3 calcNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    vec3 n = vec3(
        length(p + e.xyy) - length(p - e.xyy),
        length(p + e.yxy) - length(p - e.yxy),
        length(p + e.yyx) - length(p - e.yyx)
    );
    return normalize(n);
}

float torus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float sphere(vec3 p, float r) {
    return length(p) - r;
}

float opUnion(float d1, float d2) {
    return min(d1, d2);
}

float map(vec3 p) {
    float d = 1000.0;
    
    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++) {
            vec3 torusPos = vec3(float(i) * 3.0, 0.0, float(j) * 3.0);
            
            float time = iTime * 2.0 + float(i * j);
            float bounce = abs(sin(time)) * 1.5;
            vec3 spherePos = torusPos + vec3(0.0, bounce, 0.0);
            
            float dTorus = torus(p - torusPos, vec2(1.0, 0.3));
            float dSphere = sphere(p - spherePos, 0.3);
            
            d = opUnion(d, opUnion(dTorus, dSphere));
        }
    }
    
    return d;
}

vec3 lighting(vec3 p, vec3 normal, vec3 lightPos) {
    vec3 lightDir = normalize(lightPos - p);
    float diff = max(dot(normal, lightDir), 0.0);
    return vec3(1.0, 0.8, 0.6) * diff;
}

float raymarch(vec3 ro, vec3 rd) {
    float dist = 0.0;
    float d;
    for (int i = 0; i < 100; i++) {
        vec3 p = ro + rd * dist;
        d = map(p);
        if (d < 0.001) break;
        dist += d;
        if (dist > 50.0) break;
    }
    return dist;
}

vec3 triplanar(vec3 p, sampler2D tex) {
    vec3 normal = calcNormal(p);

    vec3 weight = abs(normal);
    weight /= (weight.x + weight.y + weight.z);  // Нормализуем веса

    vec3 texXY = texture(tex, p.xy).rgb;
    vec3 texYZ = texture(tex, p.yz).rgb;
    vec3 texXZ = texture(tex, p.xz).rgb;

    return texXY * weight.z + texYZ * weight.x + texXZ * weight.y;
}
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord.xy - iResolution.xy * 0.5) / iResolution.y;
    uv.y = -uv.y;
    float camTime = iTime * 0.5;

    vec3 ro = vec3(7.0 * sin(camTime), 3.0 + sin(camTime * 0.5), 7.0 * cos(camTime));
    vec3 lookAt = vec3(0.0, 0.0, 0.0);
    vec3 forward = normalize(lookAt - ro);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, forward);

    vec3 rd = normalize(forward + uv.x * right + uv.y * up);
    float dist = raymarch(ro, rd);
    vec3 color = vec3(0.0);

    if (dist < 50.0) {
        vec3 p = ro + rd * dist;
        vec3 normal = calcNormal(p);

        vec3 lightPos = vec3(5.0 * sin(iTime), 5.0, 5.0 * cos(iTime));
        vec3 lightColor = lighting(p, normal, lightPos);

        vec3 textureColor = triplanar(p, iChannel0);
        
        color = textureColor * lightColor;
    } else {
        color = vec3(1, 0.5, 0.5);
    }

    fragColor = vec4(color, 1.0);
 
}

void main( )
{
  ivec2 fragCoord = ivec2(gl_FragCoord.xy);

  iResolution = vec2(pushed_params.resolution_x, pushed_params.resolution_y);
  iTime = pushed_params.time;

  mainImage(fragColor, fragCoord);
}