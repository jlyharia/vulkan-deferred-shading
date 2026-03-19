//
// Created by johnny on 3/16/26.
//

#include "MeshUtils.hpp"

#include <vector>
#include <glm/glm.hpp>

/**
 * Generates a UV Sphere using a "Sectors and Stacks" (latitude/longitude) approach.
 * @param radius   Distance from the center to the surface.
 * @param sectors  Number of horizontal slices (longitude lines).
 * @param stacks   Number of vertical slices (latitude lines).
 * @param vertices Output vector to append vertex data.
 * @param indices  Output vector to append index data.
 */
namespace vk_mesh {

void generateSphere(const float radius,
                    const uint32_t sectors,
                    const uint32_t stacks,
                    std::vector<Vertex> &vertices,
                    std::vector<uint32_t> &indices) {

    const auto baseIndex = static_cast<uint32_t>(vertices.size());

    const float sectorStep = 2 * M_PI / sectors;
    const float stackStep = M_PI / stacks;

    // 1. GENERATE VERTICES
    for (uint32_t i = 0; i <= stacks; ++i) {
        const float stackAngle = M_PI / 2 - i * stackStep; // pi/2 to -pi/2
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (uint32_t j = 0; j <= sectors; ++j) {
            float sectorAngle = j * sectorStep; // 0 to 2pi

            vertices.push_back({});
            auto &v = vertices.back();

            // Position (Matches your .pos)
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            v.pos = {x, y, z};

            // Normal (.normal)
            v.normal = glm::normalize(v.pos);

            // UV (.uv)
            v.uv = {static_cast<float>(j) / sectors, static_cast<float>(i) / stacks};

            // Color (.color) - Default White
            v.color = {1.0f, 1.0f, 1.0f};

            // Tangent (.tangent) - Derived from the derivative of the position
            // This allows normal mapping to work on the sphere.
            v.tangent = glm::vec4(-sinf(sectorAngle), cosf(sectorAngle), 0.0f, 1.0f);
        }
    }

    // 2. GENERATE INDICES
    for (uint32_t i = 0; i < stacks; ++i) {
        uint32_t k1 = baseIndex + i * (sectors + 1);
        uint32_t k2 = k1 + sectors + 1;

        for (uint32_t j = 0; j < sectors; ++j, ++k1, ++k2) {
            // Triangle 1
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            // Triangle 2
            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}

} // namespace vk_mesh
