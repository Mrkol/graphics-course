#define MAX_STEPS 120
#define MAX_DIST 50.0
#define SURF_DIST 0.001

float crazyLength(vec3 p)
{
    float l2 = length(p);
    float l4 = pow(abs(p.x), 4.0) + pow(abs(p.y), 4.0) + pow(abs(p.z), 4.0);
    l4 = pow(l4, 0.25);
    
    float wave = sin(5.0*p.x + iTime) *
                 sin(5.0*p.y + iTime) *
                 sin(5.0*p.z + iTime) * 0.2;
    
    return mix(l2, l4, 0.7) + wave;
}

// SDF
float map(vec3 p)
{
    // вращение со временем
    float a = iTime * 0.5;
    mat2 rot = mat2(cos(a), -sin(a),
                    sin(a),  cos(a));
    p.xz = rot * p.xz;
    
    return crazyLength(p) - 1.0;
}

float rayMarch(vec3 ro, vec3 rd)
{
    float dO = 0.0;
    
    for(int i = 0; i < MAX_STEPS; i++)
    {
        vec3 p = ro + rd * dO;
        float dS = map(p);
        dO += dS;
        
        if(abs(dS) < SURF_DIST || dO > MAX_DIST) break;
    }
    
    return dO;
}

vec3 getNormal(vec3 p)
{
    vec2 e = vec2(0.001, 0.0);
    
    return normalize(vec3(
        map(p + e.xyy) - map(p - e.xyy),
        map(p + e.yxy) - map(p - e.yxy),
        map(p + e.yyx) - map(p - e.yyx)
    ));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    
    vec3 ro = vec3(0.0, 0.0, 4.0);
    vec3 rd = normalize(vec3(uv, -1.5));
    
    float d = rayMarch(ro, rd);
    
    vec3 col = vec3(0.0);
    
    if(d < MAX_DIST)
    {
        vec3 p = ro + rd * d;
        vec3 n = getNormal(p);
        
        vec3 lightPos = vec3(3.0, 3.0, 3.0);
        vec3 l = normalize(lightPos - p);
        vec3 v = normalize(ro - p);
        vec3 r = reflect(-l, n);
        
        float diff = max(dot(n, l), 0.0);
        float spec = pow(max(dot(v, r), 0.0), 32.0);
        float ambient = 0.15;
        
        // цветовая зависимость от нормали
        vec3 baseColor = 0.5 + 0.5 * sin(n * 3.0 + iTime);
        
        col = baseColor * diff + spec + ambient;
    }
    
    fragColor = vec4(col, 1.0);
}