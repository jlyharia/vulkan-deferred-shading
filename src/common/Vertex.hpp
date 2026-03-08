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

    // Helper function to tell Vulkan how to read this struct
    static vk::VertexInputBindingDescription getBindingDescription() {
        // C++ style constructor: (binding, stride, inputRate)
        return vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
    }

    static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions{};

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

        return attributeDescriptions;
    }

    bool operator==(const Vertex &other) const {
        return pos == other.pos &&
               color == other.color &&
               normal == other.normal &&
               uv == other.uv;
    }
};

namespace std {
template <> struct hash<Vertex> {
    size_t operator()(Vertex const &vertex) const noexcept {
        // Using bit-shifting and XOR to combine hashes of vertex components
        return ((hash<glm::vec3>()(vertex.pos) ^
                 (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.uv) << 1) ^
               (hash<glm::vec3>()(vertex.normal) << 1);
    }
};
} // namespace std