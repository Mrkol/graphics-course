#version 430
#extension GL_GOOGLE_include_directive : require


layout(location = 0) out vec4 out_fragColor;

layout(push_constant) uniform params_t
{
  uvec2 resolution;
} params;

const float iTime = 1.0;


// https://www.shadertoy.com/view/MsXSzM

#define TIMESCALE 0.25 
#define TILES 8
#define COLOR 0.7, 1.6, 2.8

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(128, 128);
	uv.x *= 128 / 128;
	
	vec4 noise = vec4(72897, 1273498, 7129347, 61251);
	float p = 1.0 - mod(noise.r + noise.g + noise.b + iTime * float(TIMESCALE), 1.0);
	p = min(max(p * 3.0 - 1.8, 0.1), 2.0);
	
	vec2 r = mod(uv * float(TILES), 1.0);
	r = vec2(pow(r.x - 0.5, 2.0), pow(r.y - 0.5, 2.0));
	p *= 1.0 - pow(min(1.0, 12.0 * dot(r, r)), 2.0);
	
	out_fragColor = vec4(COLOR, 1.0) * p;
}
