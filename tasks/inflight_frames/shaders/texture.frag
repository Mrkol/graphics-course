#version 430

layout(push_constant) uniform params_t
{
  float iTime;
  uvec2 iResolution;
}
params;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    fragCoord.x *= (float(params.iResolution.y) / params.iResolution.x);
    float step = 60.;

    vec3 color = mix(vec3(0.2, 0.3, 0.7), vec3(0.8, 0.5, 0.2), sin(20.0 * 1.1 - params.iTime * 2.0) * 0.5 + 0.5);
    fragColor = vec4(color, 1.0);
}
