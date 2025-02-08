#pragma once

#include "Texture.hpp"
#include <glm/vec4.hpp>

struct Material {
    enum class Id : uint32_t { Invalid = ~uint32_t{0} };
    
    Texture::Id baseColorTexture         = Texture::Id::Invalid;
    Texture::Id normalTexture            = Texture::Id::Invalid;
    Texture::Id metallicRoughnessTexture = Texture::Id::Invalid;
    Texture::Id emissiveFactorTexture    = Texture::Id::Invalid;

    glm::vec4 baseColor  = {0.5f, 0.5f, 0.5f, 1.f};
    glm::vec4 EMR_Factor = {0.f, 0.f, 0.f, 0.f};
};
