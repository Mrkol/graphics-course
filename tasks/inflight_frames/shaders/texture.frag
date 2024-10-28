#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform params {
  uvec2 iResolution;
  float iTime;
};

#define PI 3.141592
float sdHexagram( in vec2 p, in float r )
{
    const vec4 k = vec4(-0.5,0.8660254038,0.5773502692,1.7320508076);
    p = abs(p);
    p -= 2.0*min(dot(k.xy,p),0.0)*k.xy;
    p -= 2.0*min(dot(k.yx,p),0.0)*k.yx;
    p -= vec2(clamp(p.x,r*k.z,r*k.w),r);
    return length(p)*sign(p.y);
}

void main() {
    vec2 uv = (vec2(gl_FragCoord) - .5 * vec2(iResolution)) / float(iResolution.y);  
    uv.y *= -1.;

    vec4 color = vec4(0.);
    color.x = sin(.2 * iTime);
    color.y = cos(.4 * iTime);
    color.z = sin(.6 * iTime);

    float d = sin(sdHexagram(uv, 10.) * 100. - iTime) ;
    color *=  d * .5;

    outColor = color; 
}