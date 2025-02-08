#ifndef MODEL_BAKERY_BAKER_VERTEXATTRIBUTES_HPP
#define MODEL_BAKERY_BAKER_VERTEXATTRIBUTES_HPP

#include <cstddef>
#include <glm/glm.hpp>



struct VertexAttrs {
    [[gnu::packed]] 
    glm::vec3   cord;
    uint32_t norm;
    
    glm::vec2 texture;
    uint32_t tangent;

    std::byte _pad2[4];
    glm::vec2 normTexCoord;
    std::byte _pad3[8];

};

static_assert(sizeof(VertexAttrs) == 48);
static_assert(offsetof(VertexAttrs, cord) == 0);
static_assert(offsetof(VertexAttrs, norm) == 12);
static_assert(offsetof(VertexAttrs, texture) == 16);
static_assert(offsetof(VertexAttrs, tangent) == 24);

#endif /* MODEL_BAKERY_BAKER_VERTEXATTRIBUTES_HPP */
