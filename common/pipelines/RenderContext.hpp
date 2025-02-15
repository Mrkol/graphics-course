#pragma once

#include <glm/glm.hpp>
#include "scene/SceneManager.hpp"
namespace pipes {

struct RenderContext {
    glm::mat4x4 worldViewProj;
    glm::mat4x4 worldView;
    glm::mat4x4 worldProj;
    glm::vec3 camPos;
    double frameTime = 0.;
    SceneManager* sceneMgr;
    glm::uvec2 resolution;
};
}