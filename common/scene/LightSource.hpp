#pragma once
#include <glm/vec4.hpp>

struct LightSource {
    enum class Id : uint32_t { Invalid = ~uint32_t{0} };

    glm::vec4 position;
    glm::vec4 colorRange;
    float visibleRadius;
    glm::vec4 floatingAmplitude = {0.f, 0.f, 0.f, 0.f};
    glm::vec4 floatingSpeed     = {0.f, 0.f, 0.f, 0.f};
};