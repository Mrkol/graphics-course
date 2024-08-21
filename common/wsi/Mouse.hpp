#pragma once

#include <array>
#include <glm/glm.hpp>

#include "wsi/MouseButton.hpp"
#include "wsi/ButtonState.hpp"


struct Mouse
{
  std::array<ButtonState, static_cast<std::size_t>(MouseButton::COUNT)> buttons{ButtonState::Low};

  glm::vec2 freePos = {0, 0};

  // Provided on a per-frame basis, but only when mouse is captured.
  glm::vec2 capturedPosDelta = {0, 0};

  // Horizontal scroll is a thing on touchpads
  glm::vec2 scrollDelta = {0, 0};

  ButtonState operator[](MouseButton key) { return buttons[static_cast<std::size_t>(key)]; }
};
