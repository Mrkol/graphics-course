#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out float out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;


layout(binding = 0) uniform sampler2D previous;

layout(push_constant) uniform params
{
  float amplitude;
  int frequency;
} pushConstant;


float rand(vec2 c){
	return fract(sin(dot(c.xy, vec2(12.7898,78.233)) + cos(dot(c.xy, vec2(-12315.5767, 3524.56)))) * 43718.5453);
}

float noise(vec2 p){
	vec2 ij = floor(p);
	vec2 xy = fract(p);
	xy = 3.*xy*xy-2.*xy*xy*xy;
  float a = rand((ij+vec2(0.,0.)));
	float b = rand((ij+vec2(1.,0.)));
	float c = rand((ij+vec2(0.,1.)));
	float d = rand((ij+vec2(1.,1.)));
	float x1 = mix(a, b, xy.x);
	float x2 = mix(c, d, xy.x);
	return clamp(mix(x1, x2, xy.y), 0., 1.);
}

void main(void)
{
  out_fragColor = 0.;
  if (pushConstant.frequency != 1) {
      out_fragColor += textureLod(previous, surf.texCoord, 0).x;
  }
  out_fragColor += pushConstant.amplitude * noise(pushConstant.frequency * surf.texCoord);
}