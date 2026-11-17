vec2 SignNotZero(vec2 value) {
    return mix(vec2(-1.0), vec2(1.0), greaterThanEqual(value, vec2(0.0)));
}

vec2 EncodeNormal(vec3 normal) {
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    vec2 encoded = normal.xy;
    if (normal.z < 0.0) {
        encoded = (1.0 - abs(encoded.yx)) * SignNotZero(encoded);
    }
    return encoded * 0.5 + 0.5;
}

vec3 DecodeNormal(vec2 encoded) {
    vec2 xy = encoded * 2.0 - 1.0;
    vec3 normal = vec3(xy, 1.0 - abs(xy.x) - abs(xy.y));
    if (normal.z < 0.0) {
        normal.xy = (1.0 - abs(xy.yx)) * SignNotZero(xy);
    }
    return normalize(normal);
}

vec3 ReconstructPosition(vec2 uv, float depth, mat4 inverseViewProjection) {
    vec4 world = inverseViewProjection * vec4(uv * 2.0 - 1.0, depth, 1.0);
    return world.xyz / world.w;
}
