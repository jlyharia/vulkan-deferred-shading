//
// Created by johnny on 2/27/26.
//

#pragma once
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform {
    glm::vec3 position{0.0f};
    // Initialize with an Identity Quaternion (no rotation)
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale{1.0f};

    glm::mat4 modelMatrix{1.0f};
    bool dirty = true;

    void setPosition(const glm::vec3 &pos) {
        position = pos;
        dirty = true;
    }

    // Rotates based on Euler Angles (Degrees)
    void setRotation(const glm::vec3 &eulerDegrees) {
        rotation = glm::quat(glm::radians(eulerDegrees));
        dirty = true;
    }

    // Rotates based on a specific Axis and Angle
    void setRotation(float angleDegrees, const glm::vec3 &axis) {
        rotation = glm::angleAxis(glm::radians(angleDegrees), axis);
        dirty = true;
    }

    void setScale(const glm::vec3 &s) {
        scale = s;
        dirty = true;
    }

    void updateMatrix() {
        if (!dirty) return;

        // T * R * S
        glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 R = glm::mat4_cast(rotation); // Efficiently converts Quat to Mat4
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

        modelMatrix = T * R * S;
        dirty = false;
    }
};