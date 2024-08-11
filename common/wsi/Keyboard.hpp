#pragma once

#include <array>

#include "wsi/KeyboardKey.hpp"
#include "wsi/ButtonState.hpp"


struct Keyboard
{
  std::array<ButtonState, static_cast<std::size_t>(KeyboardKey::COUNT)> keys{ButtonState::Low};

  ButtonState operator[](KeyboardKey key) const { return keys[static_cast<std::size_t>(key)]; }
};
