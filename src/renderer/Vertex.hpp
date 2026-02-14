#pragma once

#include <array>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 texCoord;

    // Helper function to tell Vulkan how to read this struct
    static vk::VertexInputBindingDescription getBindingDescription() {
        // C++ style constructor: (binding, stride, inputRate)
        return vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
    }

    static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 4> attributeDescriptions{};

        // Position: location 0, format R32G32B32 (vec3)
        attributeDescriptions[0] = vk::VertexInputAttributeDescription(
            0, // location
            0, // binding
            vk::Format::eR32G32B32Sfloat, // format
            offsetof(Vertex, pos) // offset
            );

        // Color: location 1
        attributeDescriptions[1] = vk::VertexInputAttributeDescription(
            1,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(Vertex, color)
            );

        // Normal: location 2
        attributeDescriptions[2] = vk::VertexInputAttributeDescription(
            2,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(Vertex, normal)
            );

        // TexCoord: location 3 (R32G32 is vec2)
        attributeDescriptions[3] = vk::VertexInputAttributeDescription(
            3,
            0,
            vk::Format::eR32G32Sfloat,
            offsetof(Vertex, texCoord)
            );

        return attributeDescriptions;
    }

    bool operator==(const Vertex &other) const {
        return pos == other.pos &&
               color == other.color &&
               normal == other.normal &&
               texCoord == other.texCoord;
    }
};

namespace std {
template <> struct hash<Vertex> {
    size_t operator()(Vertex const &vertex) const {
        // Using bit-shifting and XOR to combine hashes of vertex components
        return ((hash<glm::vec3>()(vertex.pos) ^
                 (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.texCoord) << 1) ^
               (hash<glm::vec3>()(vertex.normal) << 1);
    }
};
} // namespace std

// Sample data containers
inline std::vector<Vertex> vertices;
inline std::vector<uint32_t> indices;