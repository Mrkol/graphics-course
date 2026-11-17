#version 450
#extension GL_GOOGLE_include_directive : require

#include "rocks.glsl"

struct Instance {
    mat4 model;
    vec4 color;
};

layout(set = 0, binding = 0, std430) readonly buffer Instances {
    Instance instances[];
};
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 0) out vec3 worldPosition;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec2 texCoord;
layout(location = 3) out vec3 tint;

invariant gl_Position;

void main() {
    Instance instance = instances[gl_InstanceIndex];
    vec4 world = instance.model * vec4(position, 1.0);
    worldPosition = world.xyz;
    worldNormal = normalize(mat3(instance.model) * normal); // Uniform instance scale.
    texCoord = uv;
    tint = instance.color.rgb;
    gl_Position = pc.viewProjection * world;
}
