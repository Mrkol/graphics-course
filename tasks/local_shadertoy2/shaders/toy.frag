#version 430

layout (location = 0) out vec4 fragColor;

layout (binding = 0) uniform sampler2D genTexture;
layout (binding = 1) uniform sampler2D gTexture;
layout (binding = 2) uniform sampler2D skyTexture;

layout(push_constant)       uniform Parameters {
  uint iResolution_x;
  uint iResolution_y;
  float time;
} params;

vec2 iResolution;
float iTime;

// SDF for a sphere
float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

// 2D box
float sdBox(vec2 p, vec2 size) {
    vec2 d = abs(p) - size;
    return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0);
}

// 2D circle
float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

// 2D for "G"
float sdG(vec2 p) {
    float outerCircle = sdCircle(p, 0.5);
    
    float innerCircle = sdCircle(p, 0.4);
    
    float ring = max(outerCircle, -innerCircle);
    
    vec2 gapPos = p - vec2(0.1, 0.0);
    float gapRect = sdBox(gapPos, vec2(0.5, 0.1));
    ring = max(ring, -gapRect);
    
    vec2 barPos = p - vec2(0.2, -0.15);
    float barRect = sdBox(barPos, vec2(0.25, 0.05));
    float gShape = min(ring, barRect);
    
    return gShape;
}

float sdG3D(vec3 p) {
    float g2d = sdG(p.xy);
    float thickness = 0.2;
    float dist = abs(p.z) - thickness;
    return max(g2d, dist);
}

float rayTrace(vec3 ro, vec3 rd) {
    float t = 0.0;
    const float maxDistance = 100.0;
    const int maxSteps = 100;
    const float epsilon = 0.001;
    
    for(int i = 0; i < maxSteps; i++) {
        vec3 pos = ro + rd * t;
        float dist = sdG3D(pos);
        if(dist < epsilon) {
            return t;
        }
        t += dist;
        if(t > maxDistance) break;
    }
    return -1.0;
}

vec3 triplanarMapping(vec3 p, vec3 normal, sampler2D txt) {
    // Scale for texture repetition
    float scale = 2.0;

    // Absolute value of the normal vector components
    vec3 absNormal = abs(normal);

    // Compute blending weights
    vec3 weights = absNormal / (absNormal.x + absNormal.y + absNormal.z);

    // Sample textures along each axis
    vec2 uvX = p.yz * scale;
    vec2 uvY = p.xz * scale;
    vec2 uvZ = p.xy * scale;

    vec3 texX = texture(txt, uvX).rgb;
    vec3 texY = texture(txt, uvY).rgb;
    vec3 texZ = texture(txt, uvZ).rgb;

    // Blend the texture samples
    vec3 texColor = texX * weights.x + texY * weights.y + texZ * weights.z;

    return texColor;
}

vec3 getNormal(vec3 p) {
    float h = 0.0001;
    vec3 n;
    n.x = sdG3D(p + vec3(h, 0, 0)) - sdG3D(p - vec3(h, 0, 0));
    n.y = sdG3D(p + vec3(0, h, 0)) - sdG3D(p - vec3(0, h, 0));
    n.z = sdG3D(p + vec3(0, 0, h)) - sdG3D(p - vec3(0, 0, h));
    return normalize(n);
}

vec3 getLighting(vec3 p, vec3 rd) {
    vec3 lightPos = vec3(5.0, 5.0, 5.0);
    vec3 lightDir = normalize(lightPos - p);
    vec3 normal = getNormal(p);

    // Triplanar mapping to get the texture color
    vec3 texColor = triplanarMapping(p, normal, gTexture);

    // Ambient light
    vec3 ambient = 0.2 * texColor;

    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * texColor;

    // Specular shading
    vec3 viewDir = normalize(-rd);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    vec3 specular = spec * vec3(0.5);

    // Combine the lighting components
    vec3 color = ambient + diffuse + specular;

    color = color * 0.5 + triplanarMapping(p, normal, genTexture) * 0.5;

    return color;
}

vec3 getBackgroundColor(vec2 uv) {
    vec3 topColor = vec3(0.0, 0.0, 0.2);    // Dark blue
    vec3 bottomColor = vec3(0.0, 0.2, 0.5); // Lighter blue
    return mix(bottomColor, topColor, uv.y);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    // Normalize pixel coordinates
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec2 p = uv * 2.0 - 1.0;
    p.x *= iResolution.x / iResolution.y;
    
    // Camera
    vec3 ro = vec3(0.0, 0.0, 3.0);
    vec3 rd = normalize(vec3(p, -1.5));
    
    // Rotate
    float angle = iTime * 0.5;
    mat3 rotation = mat3(
        cos(angle), 0.0, sin(angle),
        0.0,        1.0, 0.0,
        -sin(angle), 0.0, cos(angle)
    );
    rd = rotation * rd;
    ro = rotation * ro;
    
    float t = rayTrace(ro, rd);
    
    vec3 color;
    if(t > 0.0) {
        vec3 pos = ro + rd * t;
        color = getLighting(pos, rd);
    } else {
    	vec3 normal = getNormal(rd);
        color = triplanarMapping(rd, normal, skyTexture).rgb;
    }
    
    fragColor = vec4(color, 1.0);
}

void main()
{
  iResolution = vec2(params.iResolution_x, params.iResolution_y);
  iTime = params.time;

  ivec2 iFragCoord = ivec2(gl_FragCoord.xy);
  mainImage(fragColor, iFragCoord);
 }
