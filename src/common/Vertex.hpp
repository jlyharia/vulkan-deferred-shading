#pragma once

#include <array>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

struct Vertex {
    glm::vec3 pos; // Location 0
    glm::vec3 normal; // Location 1
    glm::vec2 uv; // Location 2
    glm::vec3 color; // Location 3
    glm::vec4 tangent; // Location 4

    // Helper function to tell Vulkan how to read this struct
    static vk::VertexInputBindingDescription getBindingDescription() {
        // C++ style constructor: (binding, stride, inputRate)
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 5> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 5> attributeDescriptions{};

        // Location 0: Position (vec3)
        attributeDescriptions[0].setBinding(0)
                                .setLocation(0)
                                .setFormat(vk::Format::eR32G32B32Sfloat)
                                .setOffset(offsetof(Vertex, pos));

        // Location 1: Normal (vec3) - Moved from Location 2
        attributeDescriptions[1].setBinding(0)
                                .setLocation(1)
                                .setFormat(vk::Format::eR32G32B32Sfloat)
                                .setOffset(offsetof(Vertex, normal));

        // Location 2: UV (vec2) - Moved from Location 3
        attributeDescriptions[2].setBinding(0)
                                .setLocation(2)
                                .setFormat(vk::Format::eR32G32Sfloat)
                                .setOffset(offsetof(Vertex, uv));

        // Location 3: Color (vec3) - Moved from Location 1
        attributeDescriptions[3].setBinding(0)
                                .setLocation(3)
                                .setFormat(vk::Format::eR32G32B32Sfloat)
                                .setOffset(offsetof(Vertex, color));

        attributeDescriptions[4].setBinding(0)
                                .setLocation(4)
                                .setFormat(vk::Format::eR32G32B32A32Sfloat)
                                .setOffset(offsetof(Vertex, tangent));
        return attributeDescriptions;
    }

    bool operator==(const Vertex &other) const {
        return pos == other.pos
               && color == other.color
               && normal == other.normal
               && uv == other.uv
               && tangent == other.tangent;

    }
};

namespace std {
template <> struct hash<Vertex> {
    size_t operator()(Vertex const &vertex) const noexcept {
        size_t h = 0;
        // Combine all fields into the hash
        h ^= hash<glm::vec3>()(vertex.pos) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= hash<glm::vec3>()(vertex.normal) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= hash<glm::vec2>()(vertex.uv) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= hash<glm::vec3>()(vertex.color) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= hash<glm::vec4>()(vertex.tangent) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};
} // namespace std