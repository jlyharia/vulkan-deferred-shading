//
// Created by johnny on 1/25/26.
//

#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <ostream>

class Camera {
public:
    // Position state
    glm::vec3 position;
    glm::vec3 forward;
    glm::vec3 up;
    glm::vec3 right;
    const glm::vec3 worldUp = glm::vec3(0.0f, 0.0f, 1.0f);

    // Orientation state (Euler Angles)
    float yaw; // Left/Right rotation
    float pitch; // Up/Down rotation

    // Camera Constants
    float movementSpeed = 2.5f;
    float mouseSensitivity = 0.02f;
    float fov = 45.0f;

    Camera(glm::vec3 startPosition = glm::vec3(-2.0f, -2.0f, 2.0f),
           float startYaw = 45.0f, float startPitch = -30.0f)
        : position(startPosition), yaw(startYaw), pitch(startPitch) {
        updateCameraVectors();
    }

    // Returns the view matrix for the UBO
    [[nodiscard]] glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + forward, up);
    }

    // Returns the projection matrix for the UBO
    [[nodiscard]] glm::mat4 getProjectionMatrix(float aspectRatio) const {
        auto proj = glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 100.0f);
        proj[1][1] *= -1; // Vulkan Y-flip
        return proj;
    }

    // Process keyboard input
    void handleInput(GLFWwindow *window, float deltaTime) {
        float velocity = movementSpeed * deltaTime;

        // Forward/Backward (WASD)
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            position += forward * velocity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            position -= forward * velocity;

        // Strafe Left/Right (WASD)
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            position -= right * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            position += right * velocity;

        // Up/Down movement (E/Q) - Flying
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            position += worldUp * velocity;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            position -= worldUp * velocity;

        // Looking Up/Down (Arrow Keys)
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            rotate(0, 1.0f);
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            rotate(0, -1.0f);

        // Looking Left/Right (Arrow Keys)
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            rotate(1.0f, 0);
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            rotate(-1.0f, 0);
        // std::cout << position.x << " " << position.y << " " << position.z
        //           << std::endl;
    }

    // Update rotation angles
    void rotate(float yawOffset, float pitchOffset) {
        yaw += yawOffset * mouseSensitivity;
        pitch += pitchOffset * mouseSensitivity;

        // Constrain pitch to prevent screen flipping (Gimbal Lock)
        pitch = std::clamp(pitch, -89.0f, 89.0f);

        updateCameraVectors();
    }

private:
    // Calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors() {
        // Calculate the new forward vector
        glm::vec3 newForward;
        newForward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newForward.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        newForward.z = sin(glm::radians(pitch));
        forward = glm::normalize(newForward);

        // Also re-calculate the Right and Up vector
        // Normalize the vectors, because their length gets closer to 0 the more you
        // look up or down which results in slower movement.
        right = glm::normalize(glm::cross(forward, worldUp));
        up = glm::normalize(glm::cross(right, forward));
    }
};