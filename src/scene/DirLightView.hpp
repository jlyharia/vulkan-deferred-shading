//
// Created by johnny on 4/5/26.
//

#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "common/Config.hpp"

struct CascadeData {
    glm::mat4 lightSpaceMatrix;
    float splitDepth;  // positive distance from camera — far plane of this cascade
};

/// Directional light parameters and per-cascade light-space matrix computation.
/// position + target define the light's view direction.
/// shadowFar is the outermost cascade distance (metres from camera), independent of the camera's infinite far plane.
struct DirLightView {
    glm::vec3 position = glm::vec3(10.0f, 10.0f, 20.0f);
    glm::vec3 target = glm::vec3(0.0f);
    // glm::vec3 position = glm::vec3(-14.0f, 36.0f, 45.0f);
    // glm::vec3 target = glm::vec3(0.0f, -24.0f, -16.0f);
    float shadowFar = 200.0f;

    [[nodiscard]] glm::mat4 viewMatrix() const {
        glm::vec3 dir = glm::normalize(target - position);
        glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
        if (glm::abs(glm::dot(dir, up)) > 0.999f)
            up = glm::vec3(0.0f, 1.0f, 0.0f); // fallback when light is near-vertical
        return glm::lookAt(position, target, up);
    }

    /// Computes per-cascade light-space matrices and split depths.
    /// cameraProj must be the camera's projection matrix (used to extract FOV and near plane).
    /// lambda blends log splits (1.0) vs uniform splits (0.0); 0.9 works well in practice.
    [[nodiscard]] std::array<CascadeData, engineConfig::NUM_CASCADES> computeCascades(
        const glm::mat4 &cameraView,
        const glm::mat4 &cameraProj,
        float lambda = 0.9f) const;
};
