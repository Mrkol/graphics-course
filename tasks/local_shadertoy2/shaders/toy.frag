#version 430

layout(push_constant) uniform params
{
  uint resolutionX;
  uint resolutionY;
  float time;
}
env;

layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D iChannel1; // object texture
layout(binding = 1) uniform sampler2D iChannel2; // procedural texture

float iTime()
{
  return env.time;
}

vec2 iResolution()
{
  return ivec2(env.resolutionX, env.resolutionY);
}

float sdBox(vec3 p, vec3 b)
{
  vec3 q = abs(p) - b;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdSphere(vec3 p, float r)
{
  return length(p) - r;
}

vec3 getLightPos()
{
  float swing = sin(iTime());
  return vec3(-1.5 + swing, -1.5 - swing, -1.5);
}

vec3 triplanarTex(vec3 p, vec3 normal) {
    normal = abs(normal);
    normal /= (normal.x + normal.y + normal.z); // нормировка весов

    vec3 tx = texture(iChannel1, p.yz).rgb;
    vec3 ty = texture(iChannel1, p.zx).rgb;
    vec3 tz = texture(iChannel1, p.xy).rgb;

    return tx * normal.x + ty * normal.y + tz * normal.z;
}

float sceneSDF(vec3 p)
{
  float box1 = sdBox(p - vec3(-2.0, -3.5, -2.0), vec3(2.0, 0.5, 2.0));
  float box2 = sdBox(p - vec3(-3.5, -2.0, -2.0), vec3(0.5, 2.0, 2.0));
  float box3 = sdBox(p - vec3(-2.0, -2.0, -3.5), vec3(2.0, 2.0, 0.5));

  float cut1 = sdSphere(p - vec3(-1.5, -3.0, -1.5), 1.0);
  float cut2 = sdSphere(p - vec3(-3.0, -1.5, -1.5), 1.0);
  float cut3 = sdSphere(p - vec3(-1.5, -1.5, -3.0), 1.0);

  vec3 lightPos = getLightPos();
  float sphere = sdSphere(p - lightPos, 0.1);

  return min(min(max(box1, -cut1), max(box2, -cut2)), min(max(box3, -cut3), sphere));
}

vec3 calcNormal(vec3 p)
{
  vec2 e = vec2(0.001, 0.0);
  return normalize(vec3(
    sceneSDF(p + e.xyy) - sceneSDF(p - e.xyy),
    sceneSDF(p + e.yxy) - sceneSDF(p - e.yxy),
    sceneSDF(p + e.yyx) - sceneSDF(p - e.yyx)));
}

vec3 phongLighting(vec3 pos, vec3 normal, vec3 viewDir)
{
  vec3 lightPos = getLightPos();
  vec3 lightDir = normalize(lightPos - pos);

  vec3 ambient = vec3(0.2, 0.2, 0.3);
  vec3 diffuse = vec3(0.5);
  vec3 specular = vec3(0.5);

  float diff = max(dot(normal, lightDir), 0.0);
  vec3 reflectDir = reflect(-lightDir, normal);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

  return ambient + diff * diffuse + spec * specular;
}

float rayMarch(vec3 ro, vec3 rd, out vec3 hitPos, out bool isLightSphere)
{
  float t = 0.0;
  isLightSphere = false;

  for (int i = 0; i < 100; i++)
  {
    vec3 p = ro + rd * t;
    float d = sceneSDF(p);

    if (d < 0.001)
    {
      hitPos = p;
      vec3 lightPos = getLightPos();
      if (length(p - lightPos) < 0.15)
      {
        isLightSphere = true;
      }
      break;
    }

    t += d;
    if (t > 50.0)
    { // увеличил дальность
      hitPos = vec3(0.0);
      break;
    }
  }
  return t;
}

mat3 setCamera(vec3 ro, vec3 ta)
{
  vec3 cw = normalize(ta - ro);
  vec3 cp = vec3(0.0, 1.0, 0.0);
  vec3 cu = normalize(cross(cw, cp));
  vec3 cv = cross(cu, cw);
  return mat3(cu, cv, cw);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution().xy) / iResolution().y;
    
    vec3 ro = vec3(2.0, 2.0, 2.0);
    vec3 ta = vec3(0.0, 0.0, 0.0);
    mat3 ca = setCamera(ro, ta);
    vec3 rd = ca * normalize(vec3(uv, 1.5));
    
    vec3 hitPos;
    bool isLightSphere;
    float t = rayMarch(ro, rd, hitPos, isLightSphere);
    
    if (t < 20.0) {
        if (isLightSphere) {
            fragColor = vec4(0.8, 0.8, 0.8, 1.0);
        } else {
            vec3 normal = calcNormal(hitPos);
            vec3 viewDir = normalize(ro - hitPos);
            vec3 procTex = texture(iChannel2, hitPos.xy * 0.2).rgb;
            vec3 texColor = triplanarTex(hitPos, normal);

            texColor = mix(texColor, texColor * procTex, 0.5);

            vec3 color = phongLighting(hitPos, normal, viewDir) * texColor;
            fragColor = vec4(color, 1.0);
        }
    } else {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}

void main() {
    vec2 scale = 3.0 * iResolution().xy / max(iResolution().x, iResolution().y);
    vec2 uv = scale * (gl_FragCoord.xy / iResolution().xy - vec2(0.25, 0.25)) * iResolution().xy;

    mainImage(outColor, uv);
}