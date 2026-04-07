//
// Created by johnny on 4/5/26.
//

#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

/// Directional light parameters and light-space matrix computation.
/// position + target define the light's view; orthoSize, nearPlane, farPlane define the shadow frustum.
struct DirLightView {
    glm::vec3 position  = glm::vec3(10.0f, 10.0f, 20.0f);
    glm::vec3 target    = glm::vec3(0.0f);
    float orthoSize     = 20.0f; // half-width/height of the ortho frustum
    float nearPlane     = 0.1f;
    float farPlane      = 100.0f;

    [[nodiscard]] glm::mat4 viewMatrix() const {
        glm::vec3 dir = glm::normalize(target - position);
        glm::vec3 up  = glm::vec3(0.0f, 0.0f, 1.0f);
        if (glm::abs(glm::dot(dir, up)) > 0.999f)
            up = glm::vec3(0.0f, 1.0f, 0.0f); // fallback when light is near-vertical
        return glm::lookAt(position, target, up);
    }

    [[nodiscard]] glm::mat4 projMatrix() const {
        glm::mat4 proj = glm::orthoRH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
        proj[1][1] *= -1; // Vulkan Y-flip
        return proj;
    }

    [[nodiscard]] glm::mat4 lightSpaceMatrix() const { return projMatrix() * viewMatrix(); }
};
