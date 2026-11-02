#version 450

layout(set = 0, binding = 0, std430) readonly buffer RockVertices {
    vec4 vertices[];
};

struct Instance {
    mat4 transform;
    vec4 color;
};

layout(set = 0, binding = 1, std430) readonly buffer RockInstances {
    Instance instances[];
};

layout(set = 0, binding = 2) uniform sampler2D heightMap;

layout(push_constant) uniform DrawSettings {
    mat4 viewProj;
    vec4 light;
    vec4 eye;
    vec4 detail;
    vec4 levels;
} params;

layout(location = 0) out vec3 oWorld;
layout(location = 1) out vec3 oNormal;
layout(location = 2) out vec4 oColor;

void main() {
    vec3 pos = vertices[gl_VertexIndex * 2].xyz;
    vec3 normal = vertices[gl_VertexIndex * 2 + 1].xyz;
    Instance instance = instances[gl_InstanceIndex];
    vec3 world = (instance.transform * vec4(pos, 1.0)).xyz;
    vec2 uv = clamp(instance.transform[3].xz / 100.0 + 0.5, vec2(0.0), vec2(1.0));
    world.y += max(textureLod(heightMap, uv, 0.0).r, 0.0);
    oWorld = world;
    oNormal = normalize(mat3(instance.transform) * normal);
    oColor = instance.color;
    gl_Position = params.viewProj * vec4(world, 1.0);
}
