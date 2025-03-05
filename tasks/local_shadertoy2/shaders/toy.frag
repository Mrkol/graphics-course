#version 430
#extension GL_GOOGLE_include_directive:require

#define PI 3.14159265359
#define E 2.7182818284

// layout(local_size_x = 32, local_size_y = 32) in;

layout(binding=0)uniform sampler2D iChannel0;
layout(binding=1)uniform sampler2D fileTexture;

layout(location=0)out vec4 out_fragColor;

layout(push_constant)uniform params
{
    uint resolution_x;
    uint resolution_y;
    float time;
}params_t;

vec2 iResolution;
vec2 iMouse;
float iTime;

vec3 eye=vec3(0,0,10);
vec3 light=vec3(4,3,5);
float ambient=.05;

vec3 yellow=vec3(1,1,0);
vec3 red=vec3(1,0,0);
vec3 black=vec3(0,0,0);

mat3 yaw(in float angle){
    float c=cos(angle);
    float s=sin(angle);
    return mat3(c,s,0,-s,c,0,0,0,1);
}

mat3 pitch(in float angle){
    float c=cos(angle);
    float s=sin(angle);
    return mat3(1,0,0,0,c,s,0,-s,c);
}

mat3 roll(in float angle){
    float c=cos(angle);
    float s=sin(angle);
    return mat3(c,0,s,0,1,0,-s,0,c);
}

int MAX_STEPS=50;
float EPS=.01;

const vec3 center=vec3(3,0,0);
const vec3 center2=vec3(0,3,0);
const float radius=1.;
float dSphere(in vec3 p){
    mat3 rotation=roll(iTime);
    p*=2.;
    p=rotation*p;
    p.z/=2.;
    p=p-center;
    return(length(p)-radius+.03*sin(20.*p.z+iTime))/2.;
}

float dSphere2(in vec3 p){
    mat3 rotation=pitch(iTime);
    p*=2.;
    p=rotation*p;
    p.z/=2.;
    p=p-center2;
    return(length(p)-radius+.03*sin(20.*p.z+iTime))/2.;
}

vec2 getTexCoord(vec3 p){
    mat3 rotation=pitch(iTime);
    p*=2.;
    p=rotation*p;
    p.z/=2.;
    
    return vec2(sin((p-center).x),cos((p-center).y));
}

float sdf(in vec3 p){
    //return dSphere(transpose(yaw(iTime)) * p - vec3(1, 1, 1), vec3(0.0));
    return min(dSphere(p),dSphere2(p));
}

vec3 trace(in vec3 st,in vec3 dir,out bool hit){
    vec3 p=st;
    hit=false;
    
    for(int i=0;i<MAX_STEPS;i++){
        float dist=sdf(p);
        p+=dir*dist;
        
        if(dist<EPS){
            hit=true;
            break;
        }
    }
    
    return p;
}

vec3 generateNormal(vec3 z)
{
    float e=.0001;
    float dx1=sdf(z+vec3(e,0,0));
    float dx2=sdf(z-vec3(e,0,0));
    float dy1=sdf(z+vec3(0,e,0));
    float dy2=sdf(z-vec3(0,e,0));
    float dz1=sdf(z+vec3(0,0,e));
    float dz2=sdf(z-vec3(0,0,e));
    
    return normalize(vec3(dx1-dx2,dy1-dy2,dz1-dz2));
}

vec3 skyboxColor(in vec3 direction){
    float zoom=cos(iTime)*5.+2.5;
    vec2 uv=gl_FragCoord.xy/iResolution.xy*zoom-zoom/2.;
    uv.x*=iResolution.x/iResolution.y;
    uv=vec2(uv.x*cos(iTime)-uv.y*cos(iTime)*cos(iTime),uv.x*cos(iTime)*cos(iTime)+uv.y*cos(iTime));
    uv=uv*uv/14;
    float r=length(uv);
    float sum=0.;
    for(int i=0;i<13;i++)
    {
        if(i<64+int(cos(iTime)*64.))
        {
            float theta1=(7.*atan(uv.y,uv.x)-r*PI*4.*cos(float(i)+iTime))+cos(iTime);
            float awesome=pow(clamp(1.-acos(cos(theta1)),0.,1.),PI);
            sum+=awesome;
        }
    }
    
    // return texture(iChannel2, direction).rgb;
    // return vec3(0.9, 0.9, 0.0);
    return vec3(cos(sum*1.+cos(iTime*1.))*.5+.5,cos(sum*1.+cos(iTime*2.))*.5+.5,cos(sum*1.+cos(iTime*3.))*.5+.5);
}

void main()
{
    iResolution=vec2(params_t.resolution_x,params_t.resolution_y);
    iTime=params_t.time;
    vec2 fragCoord=vec2(gl_FragCoord.xy);
    
    bool hit;
    vec2 uv=fragCoord/iResolution.xy;
    uv=uv*2.-1.;
    
    uv.x=uv.x*iResolution.x/iResolution.y;
    vec3 dir=normalize(vec3(uv,0)-eye);
    
    vec3 dir2;
    float near=1.;
    float fov=.6;
    dir2.z=-near;
    dir2.y=near*tan(fov/2.)*uv.y;
    dir2.x=near*tan(fov/2.)*uv.x;
    dir2=normalize(dir2);
    dir2=dir2;
    
    vec3 p=trace(eye,dir2,hit);
    vec3 color=vec3(0.);
    
    if(hit){
        vec3 ld=normalize(light-p);
        vec3 ed=normalize(eye-p);
        vec3 norm=generateNormal(p);
        vec3 h=normalize(ld+ed);
        float hn=max(0.,dot(h,norm));
        float spec=pow(hn,100.);
        float difus=max(0.,dot(norm,ld));
        float light=.3*spec+.7*difus+ambient;
        
        color=texture(iChannel0,getTexCoord(p)).xyz*vec3(light);
    }else{
        color=skyboxColor(dir2);
    }
    
    out_fragColor=vec4(color,1);
}
