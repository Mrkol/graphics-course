#version 430

layout(location = 0) out vec4 outColor;
layout(push_constant) uniform params
{
  uint resolutionX;
  uint resolutionY;
  float time;
}
env;

vec2 iResolution()
{
  return ivec2(env.resolutionX, env.resolutionY);
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
           (c - a) * u.y * (1.0 - u.x) +
           (d - b) * u.x * u.y;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution().xy * 5.0;

    float n = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < 5; i++) {
        n += amp * noise(uv * freq);
        freq *= 2.0;
        amp *= 0.5;
    }

    float marble = sin(uv.x * 3.0 + n * 6.0);

    vec3 baseColor = mix(vec3(0.8, 0.7, 0.6), vec3(0.3, 0.25, 0.2), marble * 0.5 + 0.5);

    fragColor = vec4(baseColor, 1.0);
}

void main() {
  vec2 scale = 3.0 * iResolution().xy / max(iResolution().x, iResolution().y);
  vec2 uv = scale * (gl_FragCoord.xy / iResolution().xy - vec2(0.25, 0.25)) * iResolution().xy;

  mainImage(outColor, uv);
}