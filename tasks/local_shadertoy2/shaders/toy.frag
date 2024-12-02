#version 430
#extension GL_GOOGLE_include_directive : require


layout(binding = 0) uniform sampler2D iChannel0;
layout(binding = 1) uniform sampler2D iChannel1;

layout(location = 0) out vec4 out_fragColor;

layout(push_constant) uniform params_t
{
  uvec2 resolution;
} params;


const float iTime = 10.0f;

const float PI = 3.14159265359;

const vec2 iResolution = vec2(1280, 720);
const ivec3 iMouse = ivec3(0, 1, 0);

const vec3  eye      = vec3 ( 4, 0, 2 );
const vec3  light    = vec3  ( 15.0, -1.0, 0.0 );
const int   maxSteps = 70;
const float eps      = 0.01;

// Rotation matrix around the X axis.
mat3 rotateX(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(1, 0, 0),
        vec3(0, c, -s),
        vec3(0, s, c)
    );
}

// Rotation matrix around the Y axis.
mat3 rotateY(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(c, 0, s),
        vec3(0, 1, 0),
        vec3(-s, 0, c)
    );
}

float dSphere ( vec3 p, in vec3 c )
{
	return length ( p - c ) - 1.0 + 0.03 * sin(20.0*p.x + iTime);
}

float length8 ( in vec2 p )
{
    return pow ( pow ( p.x, 8.0 ) + pow ( p.y, 8.0 ), 1.0/ 8.0 );
}

float length8 ( in vec3 p )
{
    return pow ( pow ( p.x, 8.0 ) + pow ( p.y, 8.0 ) + pow ( p.z, 8.0 ), 1.0/ 8.0 );
}


float dTorus ( vec3 p, vec2 t )
{
	vec2	q = vec2 ( length8 ( p.xz ) - t.x, p.y );
	
	return length8 ( q ) - t.y;
}

float smin ( float a, float b, float k )
{
	float res = exp ( -k*a ) + exp ( -k*b );
	return -log ( res ) / k;
}

float sdf ( in vec3 p )
{
    //return dSphere ( p, vec3 ( 0, 0, 0 ) );
    //return dBox ( p, vec3 ( 0.5, 0.2, 0.7 ) );
	return dTorus ( p, vec2 ( 0.73, 0.5 ) );
}

float sdPyramid( vec3 p, float h )
{
  float m2 = h*h + 0.25;
    
  p.xz = abs(p.xz);
  p.xz = (p.z>p.x) ? p.zx : p.xz;
  p.xz -= 0.5;

  vec3 q = vec3( p.z, h*p.y - 0.5*p.x, h*p.x + 0.5*p.y);
   
  float s = max(-q.x,0.0);
  float t = clamp( (q.y-0.5*p.z)/(m2+0.25), 0.0, 1.0 );
    
  float a = m2*(q.x+s)*(q.x+s) + q.y*q.y;
  float b = m2*(q.x+0.5*t)*(q.x+0.5*t) + (q.y-m2*t)*(q.y-m2*t);
    
  float d2 = min(q.y,-q.x*m2-q.y*0.5) > 0.0 ? 0.0 : min(a,b);
    
  return sqrt( (d2+q.z*q.z)/m2 ) * sign(max(q.z,-p.y));
}

float sdf ( in vec3 p, in mat3 m )
{
   vec3 q = m * p;
    
    //return dSphere ( p, vec3 ( 2, 0, 0 ) );
    //return dBox ( q, vec3 ( 0.5, 0.2, 0.7 ) );
	return sdPyramid(q, 0.9);
}

vec3 trace ( in vec3 from, in vec3 dir, out bool hit, in mat3 m )
{
	vec3	p         = from;
	float	totalDist = 0.0;
	
	hit = false;
	
	for ( int steps = 0; steps < maxSteps; steps++ )
	{
		float	dist = sdf ( p, m );
        
		if ( dist < 0.01 )
		{
			hit = true;
			break;
		}
		
		totalDist += dist;
		
		if ( totalDist > 20.0 )
			break;
			
		p += dist * dir;
	}
	
	return p;
}

vec3 generateNormal ( vec3 z, float d, in mat3 m )
{
    float e   = max (d * 0.5, eps );
    float dx1 = sdf(z + vec3(e, 0, 0), m);
    float dx2 = sdf(z - vec3(e, 0, 0), m);
    float dy1 = sdf(z + vec3(0, e, 0), m);
    float dy2 = sdf(z - vec3(0, e, 0), m);
    float dz1 = sdf(z + vec3(0, 0, e), m);
    float dz2 = sdf(z - vec3(0, 0, e), m);
    
    return normalize ( vec3 ( dx1 - dx2, dy1 - dy2, dz1 - dz2 ) );
}

const float roughness = 0.2;
const vec3  r0   = vec3 ( 1.0, 0.92, 0.23 );
const vec3  clr  = vec3 ( 0.7, 0.7, 0.5 );
const float gamma = 10.0;
const float pi    = 3.1415926;
const float FDiel = 0.04;		// Fresnel for dielectrics

vec3 fresnel ( in vec3 f0, in float product )
{
	product = clamp ( product, 0.0, 1.0 );		// saturate
	
	return mix ( f0, vec3 (1.0), pow(1.0 - product, 5.0) );
}

float D_blinn(in float roughness, in float NdH)
{
    float m = roughness * roughness;
    float m2 = m * m;
    float n = 2.0 / m2 - 2.0;
    return (n + 2.0) / (2.0 * pi) * pow(NdH, n);
}

float D_beckmann ( in float roughness, in float NdH )
{
	float m    = roughness * roughness;
	float m2   = m * m;
	float NdH2 = NdH * NdH;
	
	return exp( (NdH2 - 1.0) / (m2 * NdH2) ) / (pi * m2 * NdH2 * NdH2);
}

float D_GGX ( in float roughness, in float NdH )
{
	float m  = roughness * roughness;
	float m2 = m * m;
	float NdH2 = NdH * NdH;
	float d  = (m2 - 1.0) * NdH2 + 1.0;
	
	return m2 / (pi * d * d);
}

float G_schlick ( in float roughness, in float nv, in float nl )
{
    float k = roughness * roughness * 0.5;
    float V = nv * (1.0 - k) + k;
    float L = nl * (1.0 - k) + k;
	
    return 0.25 / (V * L);
}

float G_neumann ( in float nl, in float nv )
{
	return nl * nv / max ( nl, nv );
}

float G_klemen ( in float nl, in float nv, in float vh )
{
	return nl * nv / (vh * vh );
}

float G_default ( in float nl, in float nh, in float nv, in float vh )
{
	return min ( 1.0, min ( 2.0*nh*nv/vh, 2.0*nh*nl/vh ) );
}

vec4 cookTorrance ( in vec3 p, in vec3 n, in vec3 l, in vec3 v, vec3 base)
{
    vec3  h    = normalize ( l + v );
	float nh   = dot (n, h);
	float nv   = dot (n, v);
	float nl   = dot (n, l);
	float vh   = dot (v, h);
    float metallness = 1.0;
//     vec3  base  = pow ( clr, vec3 ( gamma ) );
    vec3  F0    = mix ( vec3(FDiel), clr, metallness );
	
			// compute Beckman
   	float d = D_beckmann ( roughness, nh );

            // compute Fresnel
    vec3 f = fresnel ( F0, nv );
	
            // default G
    float g = G_default ( nl, nh, nv, vh );
	
			// resulting color
	vec3  ct   = f*(0.25 * d * g / nv);
	vec3  diff = max(nl, 0.0) * ( vec3 ( 1.0 ) - f ) / pi;
	float ks   = 0.5;

	return vec4 ( pow ( diff * base + ks * ct, vec3 ( 1.0 / gamma ) ), 1.0 );
}

vec4 boxmap( in sampler2D s, in vec3 p, in vec3 n, in float k )
{
    // project+fetch
    vec4 x = texture( s, p.yz );
    vec4 y = texture( s, p.zx );
    vec4 z = texture( s, p.xy );
    
    // blend weights
    vec3 w = pow( abs(n), vec3(k) );
    // blend and return
    return (x*w.x + y*w.y + z*w.z) / (w.x + w.y + w.z);
}

void main()
{

    vec4 fragColor;
    vec2 fragCoord = gl_FragCoord.xy;

        // Normalized pixel coordinates (from 0 to 1)
    bool hit;
	vec3 mouse = vec3(iMouse.xy/iResolution.xy - 0.5,iMouse.z-.5);
    mat3 m     = rotateX ( 6.0*mouse.y ) * rotateY ( 6.0*mouse.x);
    vec2 scale = 9.0 * iResolution.xy / max ( iResolution.x, iResolution.y ) ;
    vec2 uv    = scale * ( fragCoord/iResolution.xy - vec2 ( 0.5 ) );
	vec3 dir   = normalize ( vec3 ( uv, 0 ) - eye );
    vec4 color = vec4 ( 0, 0, 0, 1 );
    vec3 p     = trace ( eye, dir, hit, m );

    // vec3  base  = pow ( clr, vec3 ( gamma ) );


	if ( hit )
	{
		vec3  l  = normalize        ( light - p );
        vec3  v  = normalize        ( eye - p );
		vec3  n  = generateNormal   ( p, 0.001, m );
		float nl = max ( 0.0, dot ( n, l ) );
        vec3  h  = normalize ( l + v );
        float hn = max ( 0.0, dot ( h, n ) );
        float sp = pow ( hn, 150.0 );
		
        color = cookTorrance ( p, n, l, v,  boxmap(iChannel0, m * p, generateNormal(p, 0.5, m), 0.5).xyz);
	} else {
        color = texture(iChannel1, uv);
    }

    // Output to screen
    out_fragColor = color;
}
