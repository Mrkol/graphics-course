layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 chunk;
    vec4 terrain;
} pc;

layout(set = 0, binding = 0) uniform sampler2D heightmap;

float SampleHeight(vec2 worldXZ) {
    vec2 size = vec2(textureSize(heightmap, 0));
    vec2 uv = (worldXZ / pc.terrain.x * (size - 1.0) + 0.5) / size;
    return textureLod(heightmap, uv, 0.0).r * pc.terrain.y;
}
