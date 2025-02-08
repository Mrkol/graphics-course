#pragma once
#include "etna/Image.hpp"
#include <cstdint>

struct Texture {
    enum class Id : uint32_t { Invalid = ~uint32_t{0} };
    etna::Image image;
};
