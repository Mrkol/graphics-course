#version 450

layout(push_constant) uniform Parameters {
  uint iResolution_x;
  uint iResolution_y;
  float iTime;
} params;

float iTime;

vec2 iResolution;

layout (binding = 0) uniform sampler2D gTexture;

layout (location = 0) out vec4 fragColor;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    
    vec4 texFromChannel1 = texture(gTexture, uv);
    
    vec3 procPattern = vec3(uv, 0.5 + 0.5 * sin(iTime));
    
    vec3 finalColor = mix(procPattern, texFromChannel1.rgb, 0.5);
    
    fragColor = vec4(finalColor, 1.0);
}

void main()
{
  iResolution = vec2(params.iResolution_x, params.iResolution_y);
  iTime = params.iTime;

  ivec2 iFragCoord = ivec2(gl_FragCoord.xy);
  mainImage(fragColor, iFragCoord);
}
