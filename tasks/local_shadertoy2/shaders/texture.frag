#version 430
#extension GL_GOOGLE_include_directive:require

layout(location=0)out vec4 out_fragColor;

layout(push_constant)uniform params
{
    uint resolution_x;
    uint resolution_y;
    float time;
}params_t;

vec2 iResolution;
float iTime;

float colormap_red(float x){
    if(x<0.){
        return 54./255.;
    }else if(x<20049./82979.){
        return(829.79*x+54.51)/255.;
    }else{
        return 1.;
    }
}

float colormap_green(float x){
    if(x<20049./82979.){
        return 0.;
    }else if(x<327013./810990.){
        return(8546482679670./10875673217.*x-2064961390770./10875673217.)/255.;
    }else if(x<=1.){
        return(103806720./483977.*x+19607415./483977.)/255.;
    }else{
        return 1.;
    }
}

float colormap_blue(float x){
    if(x<0.){
        return 54./255.;
    }else if(x<7249./82979.){
        return(829.79*x+54.51)/255.;
    }else if(x<20049./82979.){
        return 127./255.;
    }else if(x<327013./810990.){
        return(792.02249341361393720147485376583*x-64.364790735602331034989206222672)/255.;
    }else{
        return 1.;
    }
}

vec4 colormap(float x){
    return vec4(colormap_red(x),colormap_green(x),colormap_blue(x),1.);
}

float rand(vec2 n){
    return fract(sin(dot(n,vec2(12.9898,4.1414)))*43758.5453);
}

float noise(vec2 p){
    vec2 ip=floor(p);
    vec2 u=fract(p);
    u=u*u*(3.-2.*u);
    
    float res=mix(
        mix(rand(ip),rand(ip+vec2(1.,0.)),u.x),
        mix(rand(ip+vec2(0.,1.)),rand(ip+vec2(1.,1.)),u.x),u.y);
        return res*res;
    }
    
    const mat2 mtx=mat2(.80,.60,-.60,.80);
    
    float fbm(vec2 p)
    {
        float f=0.;
        
        f+=.500000*noise(p+iTime);p=mtx*p*2.02;
        f+=.031250*noise(p);p=mtx*p*2.01;
        f+=.250000*noise(p);p=mtx*p*2.03;
        f+=.125000*noise(p);p=mtx*p*2.01;
        f+=.062500*noise(p);p=mtx*p*2.04;
        f+=.015625*noise(p+sin(iTime));
        
        return f/.96875;
    }
    
    float pattern(in vec2 p)
    {
        return fbm(p+fbm(p+fbm(p)));
    }
    
    void mainImage(out vec4 fragColor,in vec2 fragCoord)
    {
        vec2 uv=fragCoord/iResolution.x;
        float shade=pattern(uv);
        fragColor=vec4(colormap(shade).rgb,shade);
    }
    
    void main(void){
        iResolution=vec2(params_t.resolution_x,params_t.resolution_y);
        iTime=params_t.time;
        
        vec2 uv=gl_FragCoord.xy/min(iResolution.x,iResolution.y);
        
        uv=uv/iResolution.x;
        float shade=pattern(uv);
        out_fragColor=vec4(colormap(shade).rgb,shade);
        
    }