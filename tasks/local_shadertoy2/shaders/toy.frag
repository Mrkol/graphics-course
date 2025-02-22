#version 430


layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D ichannel1;
layout(binding = 1) uniform sampler2D iChannel0;
layout(push_constant) uniform params {
  uint resolution_x;
  uint resolution_y;
  float time;
} pushed_params;

float iTime;
vec2 iResolution;
const float PI = 3.14159265359;
const int MAX_STEPS = 200;
const float MAX_DIST = 100.0;
const float MIN_DIST = 0.001;
const float SHADOW_SOFTNESS = 64.0;
const float SPECULAR_POWER = 32.0;
const float AMBIENT_LIGHT = 0.2;
const float BALL_RADIUS = 0.4;
const float BALL_SPEED = 0.7;
const float BALL_ROTATION_SPEED = 1.2;

vec3 lightPos = vec3(4.0, 6.0, 8.0);

mat3 rotateY(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(vec3(c, 0.0, -s), vec3(0.0, 1.0, 0.0), vec3(s, 0.0, c));
}

vec3 triplanarMapping(vec3 p, vec3 normal, sampler2D tex) {
    vec3 blend = abs(normal);
    blend = blend / (blend.x + blend.y + blend.z); 

    vec3 xProj = texture(tex, p.yz).rgb;  
    vec3 yProj = texture(tex, p.xz).rgb;
    vec3 zProj = texture(tex, p.xy).rgb;

    return blend.x * xProj + blend.y * yProj + blend.z * zProj;
}

vec3 getSkyboxColor(vec3 dir) {
    return vec3(0.5, 0.5, 0.5);
}

float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

vec3 ballTrajectory(float time, float index) {
    float jumpHeight = abs(sin(time * BALL_SPEED + index)) * 3.0;
    return vec3(sin(time * BALL_SPEED + index) * 4.0, jumpHeight, cos(time * BALL_SPEED + index) * 4.0);
}

float sceneSDF(vec3 p) {
    float time = iTime * BALL_ROTATION_SPEED;
    float d = MAX_DIST;

    for (int i = 0; i < 5; i++) {
        vec3 ballPos = ballTrajectory(time, float(i) * 2.0 * PI / 5.0);
        float ballDist = sdSphere(p - ballPos, BALL_RADIUS);
        d = min(d, ballDist);
    }

    float ground = p.y + 0.4;
    d = min(d, ground);

    return d;
}

vec3 getNormal(vec3 p) {
    float h = MIN_DIST;
    vec3 n = vec3(
        sceneSDF(p + vec3(h, 0, 0)) - sceneSDF(p - vec3(h, 0, 0)),
        sceneSDF(p + vec3(0, h, 0)) - sceneSDF(p - vec3(0, h, 0)),
        sceneSDF(p + vec3(0, 0, h)) - sceneSDF(p - vec3(0, 0, h))
    );
    return normalize(n);
}

vec3 rayMarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    for (int i = 0; i < MAX_STEPS; i++) {
        vec3 p = ro + rd * t;
        float dist = sceneSDF(p);
        if (dist < MIN_DIST) {
            return p;
        }
        t += dist;
        if (t > MAX_DIST) break;
    }
    return vec3(MAX_DIST);
}

float softShadow(vec3 ro, vec3 rd, float k) {
    float res = 1.0;
    float t = 0.02;
    for (int i = 0; i < MAX_STEPS; i++) {
        float h = sceneSDF(ro + rd * t);
        if (h < MIN_DIST) return 0.0;
        res = min(res, k * h / t);
        t += h;
        if (t > MAX_DIST) break;
    }
    return res;
}

vec3 phongLighting(vec3 p, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(lightPos - p);

    vec3 ambient = AMBIENT_LIGHT * vec3(0.2, 0.8, 0.2);

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), SPECULAR_POWER);
    vec3 specular = spec * vec3(1.0);

    float shadow = softShadow(p, lightDir, SHADOW_SOFTNESS);

    return (ambient + shadow * (diffuse + specular));
}

vec3 getCameraPosition(float time) {
    float radius = 10.0;
    float height = 4.0;
    return vec3(sin(time) * radius, height, cos(time) * radius);
}

vec3 getCameraTarget() {
    return vec3(0.0, 1.0, 0.0);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    uv.y = -uv.y;
    float time = iTime * 0.2;
    vec3 ro = getCameraPosition(time);
    vec3 lookAt = getCameraTarget();

    vec3 forward = normalize(lookAt - ro);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, forward);
    vec3 rd = normalize(forward + uv.x * right + uv.y * up);

    vec3 p = rayMarch(ro, rd);

    if (length(p) < MAX_DIST) {
        vec3 normal = getNormal(p);
        vec3 viewDir = normalize(ro - p);

        vec3 ballColor = triplanarMapping(p, normal, iChannel0);

        vec3 color = phongLighting(p, normal, viewDir) * ballColor;
        fragColor = vec4(color, 1.0);
    } else {
        fragColor = vec4(getSkyboxColor(rd), 1.0);
    }
}


void main( )
{
  ivec2 fragCoord = ivec2(gl_FragCoord.xy);

  iResolution = vec2(pushed_params.resolution_x, pushed_params.resolution_y);
  iTime = pushed_params.time;

  mainImage(fragColor, fragCoord);
}