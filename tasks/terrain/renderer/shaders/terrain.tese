#version 450 core

layout (quads, fractional_odd_spacing, ccw) in;

layout(push_constant) uniform params_t
{
  vec2 base; 
  vec2 extent;
  mat4 mProjView;
  vec3 camPos;
  int degree;
} params;


layout(binding = 0) uniform sampler2D heightMap;

layout(location = 0) in vec2 TextureCoord[];

layout (location = 0) out TSE_OUT
{
  vec2 texCoord;
  vec4 normal;
  float height;
} surf;

void main()
{
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;


    vec2 t00 = TextureCoord[0];
    vec2 t01 = TextureCoord[1];
    vec2 t10 = TextureCoord[2];
    vec2 t11 = TextureCoord[3];
    
    float tstep = (t01.y - t00.y) / params.degree; 
    float pstep =              1. / params.degree; 

    vec2 t0 = (t01 - t00) * u + t00;
    vec2 t1 = (t11 - t10) * u + t10;
    vec2 t  = (t1 - t0) * v + t0;

    vec4 p00 = gl_in[0].gl_Position;
    vec4 p01 = gl_in[1].gl_Position;
    vec4 p10 = gl_in[2].gl_Position;
    vec4 p11 = gl_in[3].gl_Position;
    
    vec4 p0 = (p01 - p00) * u + p00;
    vec4 p1 = (p11 - p10) * u + p10;
    vec4 p = (p1 - p0) * v + p0;

    float h   = texture(heightMap, t  ).r * 64.0 - 16.0;
    float hn0 = texture(heightMap, t + vec2(tstep,    0)).r * 64.0 - 16.0;
    float hn1 = texture(heightMap, t + vec2(   0, tstep)).r * 64.0 - 16.0;

    vec4 pn0 = p + vec4(pstep, 0, 0, 0);
    vec4 pn1 = p + vec4(0, 0, pstep, 0);

    vec4 vertical = vec4(0, 0.5, 0, 0);

    p   += vertical * h;
    pn0 += vertical * hn0;
    pn1 += vertical * hn1;

    vec4 normal = normalize(vec4(cross((pn0 - p).xyz, (pn1-p).xyz), 0));
    //vec4 normal = vec4(0, 1, 0, 0);
    //vec4 normal = normalize(pn0 - p);

    surf.texCoord = t;
    surf.height   = h;
    surf.normal   = normal;

    gl_Position = params.mProjView * p;
}