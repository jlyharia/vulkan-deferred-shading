//
// Created by johnny on 3/16/26.
//

#include "MeshUtils.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace MeshUtils {

void generateSphere(float radius, int sectorCount, int stackCount,
                    std::vector<Vertex> &vertices, std::vector<uint32_t> &indices) {
    vertices.clear();
    indices.clear();

    const float sectorStep = 2.0f * glm::pi<float>() / static_cast<float>(sectorCount);
    const float stackStep  = glm::pi<float>() / static_cast<float>(stackCount);

    for (int i = 0; i <= stackCount; ++i) {
        const float stackAngle = glm::pi<float>() / 2.0f - static_cast<float>(i) * stackStep;
        const float xy         = radius * std::cos(stackAngle);
        const float z          = radius * std::sin(stackAngle);

        for (int j = 0; j <= sectorCount; ++j) {
            const float sectorAngle = static_cast<float>(j) * sectorStep;

            Vertex v{};
            v.pos    = glm::vec3(xy * std::cos(sectorAngle), z, xy * std::sin(sectorAngle));
            v.normal = glm::normalize(v.pos);
            v.uv     = glm::vec2(static_cast<float>(j) / static_cast<float>(sectorCount),
                                 static_cast<float>(i) / static_cast<float>(stackCount));
            v.color  = glm::vec3(1.0f);

            // Tangent: derivative of position with respect to sector angle
            glm::vec3 t(-std::sin(sectorAngle), 0.0f, std::cos(sectorAngle));
            v.tangent = glm::vec4(glm::normalize(t), 1.0f);

            vertices.push_back(v);
        }
    }

    for (int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1);
        int k2 = k1 + sectorCount + 1;

        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != stackCount - 1) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}

} // namespace MeshUtils
