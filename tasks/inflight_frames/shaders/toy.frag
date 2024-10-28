#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D iChannel1;
layout(binding = 1) uniform sampler2D iChannel2;

layout(push_constant) uniform params {
  uvec2 iResolution;
  uvec2 iMouse;
  float iTime;
};

#define NUM_LIGHT_SOURCES 2
#define MAX_SHADOWS_STEPS 80
#define MAX_MARCHING_STEPS 120
#define MARCH_STEP_COEF 0.7
#define MIN_DIST 0.0
#define MAX_DIST 8.
#define SCALE 3.

#define EPS 0.001
#define PI 3.141592

float Torus( vec3 p, vec2 t )
{
  vec2 q = vec2(length(p.xz) - t.x, p.y);
  return length(q) - t.y
      + 0.04 * sin(20.0*p.y + iTime )
      + 0.08 * sin(10.0*p.x + iTime )
      + 0.08 * sin(5.0*p.z + iTime);
}

float sdTorus( vec3 p, vec2 t )
{
  vec2 q = vec2(length(p.xz)-t.x,p.y);
  return length(q)-t.y;
}

float sdPlane( vec3 p, vec3 n, float h )
{
  return dot(p,n) + h;
}


// Rotation matrix around the axis.
mat3 rotateX(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(1, 0, 0),
        vec3(0, c, -s),
        vec3(0, s, c)
    );
}
mat3 rotateY(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(c, 0, s),
        vec3(0, 1, 0),
        vec3(-s, 0, c)
    );
}
mat3 rotateZ(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(c, -s, 0),
        vec3(s, c, 0),
        vec3(0, 0, 1)
    );
}

struct Light
{
    vec3 color;
    vec3 pos;
    float intensity;
};

struct Material 
{
    vec3 F0;
    vec3 base_color;
    float roughness;
    float reflectance;
    float gamma;
};

// operations
vec2 Union( vec2 d1, vec2 d2 )
{
	return (d1.x < d2.x) ? d1 : d2;
}

vec2 smoothUnion( vec2 d1, vec2 d2, float k )
{
    float h = clamp( 0.5 + 0.5*(d2.x-d1.x)/k, 0.0, 1.0 );
    return vec2( mix( d2.x, d1.x, h ) - k * h * (1.0 - h), d1.y);
}
vec2 smoothSubtraction ( vec2 d1, vec2 d2, float k ) 
{
    float h = clamp( 0.5 - 0.5*(d1.x + d2.x) / k, 0.0, 1.0 );
    return vec2( mix( d1.x, -d2.x, h ) + k * h * (1.0 - h), d1.y); 
}


//scene
vec2 map(vec3 p)
{
    mat3 rot = rotateX(iTime * 0.2)*rotateY(iTime * 0.1)*rotateZ(iTime * 0.1);
    mat3 rot2 = rotateX( 0.2)*rotateY(iTime * 0.1)*rotateZ( 0.1);
    vec2 res = vec2( MAX_DIST, 0.0 ); // second component - material id of object
    
    float m_id1 = 1., m_id2 = 2.;
    float smooth_coef1 = .8, smooth_coef2 = 0.3;
    
    res = Union (res, vec2(sdPlane(p - vec3(0., -1.1, 0.), vec3(0., 1., 0.), 0.), m_id1));

    res = smoothUnion (res, vec2(sdTorus((p - vec3(0., -1., 0.)) * rot2 , vec2(3., .1)), m_id2), smooth_coef1);
    
    res = Union (res, vec2(Torus((p - vec3(0.2, 0., 0.2)) * rot, vec2(.5, .3)), m_id2));
    res = smoothSubtraction(res, vec2(Torus(p - vec3(0.5, 0., 0.5), vec2(.5, .3)), m_id2 ), smooth_coef2);
    
    return res;
}

vec3 generateNormal ( vec3 p)
{
    float e = EPS;
    float dx = map(p + vec3(e, 0, 0)).x - map(p - vec3(e, 0, 0)).x;
    float dy = map(p + vec3(0, e, 0)).x - map(p - vec3(0, e, 0)).x;
    float dz = map(p + vec3(0, 0, e)).x - map(p - vec3(0, 0, e)).x;   
    return normalize ( vec3 ( dx, dy, dz ) );
}

vec2 rayMarch( vec3 ro,  vec3 rd) {
  vec2 depth = vec2(MIN_DIST, 0.);
  vec2 d = vec2(0.);
  for (int i = 0; i < MAX_MARCHING_STEPS; ++i) {
    vec3 p = ro + depth.x * rd;
    d = map(p);
    depth.x += d.x * MARCH_STEP_COEF;
    if (d.x < EPS || depth.x > MAX_DIST) 
        break;
  }
  depth.y = d.y;
  return depth;
}

//Fresnel
vec3 F_Schlick  ( in vec3 f0, in float nv )
{
    nv = clamp ( nv, 0.0, 1.0 ); 
    return mix ( f0, vec3 (1.0), pow(1.0 - nv, 5.0) );
}

//Distribution
float D_beckmann ( in float roughness, in float nh )
{
	float m    = roughness * roughness;
	float m2   = m * m;
	float nh2 = nh * nh;
	
	return exp( (nh2 - 1.0) / (m2 * nh2) ) / (PI * m2 * nh2 * nh2);
}

//Geometry
float G_default ( in float nl, in float nh, in float nv, in float vh )
{
	return min ( 1.0, min ( 2.0*nh*nv/vh, 2.0*nh*nl/vh ) );
}

vec4 cookTorrance ( Material m, in vec3 n, in vec3 l, in vec3 v, in vec3 l_color, in vec3 b_color )
{
    vec3  h = normalize ( l + v );
    
    float nh   = dot (n, h);
	float nv   = dot (n, v);
	float nl   = dot (n, l);
	float vh   = dot (v, h);
    
   	vec3 F = F_Schlick ( m.F0, nv );
    float D = D_beckmann ( m.roughness, nh );
    float G = G_default ( nl, nh, nv, vh );
    
	float  V   = 0.25 * G / ( nv * nl );
    
    m.base_color = mix( m.base_color, b_color , m.reflectance);

    vec3 ct = F * V * D * l_color;
	vec3  diff =  ( vec3(1.) - F ) * m.base_color / PI;

	return vec4 ( pow ( diff + ct, vec3 ( 1.0 / (m.gamma + EPS) ) ), 1.0 );
}

vec3 getTexture(in sampler2D channel, vec3 p, vec3 n,  float k, float scale) {
  vec3 w = abs(n);
  return 
      pow(w.x, k) * texture(channel, p.yz * scale).rgb + 
      pow(w.y, k) * texture(channel, p.xz * scale).rgb +
      pow(w.z, k) * texture(channel, p.xy * scale).rgb;
}

Material getMaterial(float id, vec3 p)
{   
    mat3 rot = rotateX(iTime * 0.2)*rotateY(iTime * 0.1)*rotateZ(iTime * 0.1);
    Material m;  
    
    if (id < 2.){
        vec3 p_ = vec3(p.x*0.4, p.y, p.z*0.4);
        return Material(
           vec3(0.04), //F0
           //vec3(0.5 * mod(floor(p.x * 5.) + floor(p.z * 5.), 2.0)), //base_color
           getTexture(iChannel2, p, generateNormal(p), 4., .2).ggg, //base_color
           .8, //roughness
           .8, //reflectance
           1. //gamma
       );
    }
       
    else
       return Material(
           vec3(0.9), //F0
           getTexture(iChannel1, p * rot, generateNormal(p * rot), 4., 1.), //base_color
           0.5, //roughness
           0.7, //reflectance
           3. //gamma
       );
}

vec4 Cubemap(in vec2 fragCoord, in vec3 rayDir, in vec3 n )
{
    rayDir = normalize(rayDir);
    vec3 col = vec3(0.);
    if(rayDir.y<0.5)
        col = vec3(.1);
    if(abs(rayDir.z) > 0.8)
        col = vec3(1., 0.8,0.8);
    else if(abs(rayDir.z) > 0.7 || abs(rayDir.z) <0.1)
        col = vec3(0.1);
    else 
        col = vec3(0.);

    col += (-rayDir) * rayDir * rayDir;
    if(abs(rayDir.z) < 0.8)
        col *= 0.3;
    return vec4(col * 0.4, 1.0);
}

vec3 getColor(vec2 res, vec3 p, vec3 ro, Light lightArray[NUM_LIGHT_SOURCES])
{
    Material m = getMaterial(res.y, p);
    vec3 color = vec3(0.);
    vec3 n = generateNormal(p); 
    vec3 v = normalize( ro - p ); // viewpoint vector
    vec2 uv = (vec2(gl_FragCoord) - .5 * vec2(iResolution)) / float(iResolution.y);  
    uv.y *= -1.;
    for(int i = 0; i < lightArray.length(); ++i){
        vec3 l = normalize(lightArray[i].pos - p); // light direction
        float shadow = 1.; 
        vec3 b_color = Cubemap(uv, reflect(p - ro, n) , n ).rgb;
        color += cookTorrance(m, n, l, v, lightArray[i].color * lightArray[i].intensity, b_color ).xyz * shadow; 
    }   
    return color;
}

vec3 render( vec2 uv, vec3 ro, vec3 rd)
{
    Light lightArray[NUM_LIGHT_SOURCES] = Light[NUM_LIGHT_SOURCES](
        Light( vec3(1., 1., 0.), vec3(0., 2., 10.), 1. ),
        Light( vec3(0.5, 0., 1.), vec3(0., 2., -10.), 1.)
        );
        
    //vec3 col = texture(iChannel0, rd).rgb; // background color
    vec3 col = Cubemap(uv, rd, rd).rgb;
    

    vec2 res = rayMarch(ro, rd); 
    if (res.x < MAX_DIST) // if hit
    {
        vec3 p = (ro + rd * res.x);
        col = getColor(res, p, ro, lightArray);
    }
    return col;
}

mat3 camera(vec3 cameraPos) {
    vec3 cd = normalize( - cameraPos); 
    vec3 cr = normalize(cross(vec3(0, 1, 0), cd)); 
    vec3 cu = normalize(cross(cd, cr)); 
    return mat3(-cr, cu, -cd);
}

void main() {
    vec2 uv = (vec2(gl_FragCoord) - .5 * vec2(iResolution)) / float(iResolution.y);  
    uv.y *= -1.;
    vec2 Mouse = vec2(iMouse) / vec2(iResolution);

    vec3 ro = vec3(0., 0., SCALE);
    ro = ro * rotateX(mix(0., -PI/3., Mouse.y)) * rotateY(mix(-PI, PI, Mouse.x ));
    vec3 rd = camera(ro) * normalize(vec3(uv, -1.));

    outColor = vec4(render(uv, ro, normalize(rd)), 1.);
}
