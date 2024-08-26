#pragma once

#include "wsi/KeyToGlfw.hpp"


// This file allows us to avoid including GLFW everywhere
enum class KeyboardKey
{
#define X(key, glfwKey) key,
  ALL_KEYBOARD_KEYS
#undef X

    COUNT,
};
