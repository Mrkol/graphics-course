#pragma once

#include <concepts>
#include "gui/IConfigurable.hpp"
#include "wsi/Keyboard.hpp"

template<typename T>
concept Pipeline = requires (T t, const Keyboard& kb) {
    t.drawGui();
    t.allocate();
    t.loadShaders();
    t.setup();
    t.debugInput(kb);
    typename T::RenderTarget;
    // t.render();
};