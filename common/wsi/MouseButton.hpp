#pragma once

#include "wsi/MouseButtonToGlfw.hpp"


enum class MouseButton
{
#define X(mb, glfwMb) mb,
  ALL_MOUSE_BUTTONS
#undef X

    COUNT,
};
