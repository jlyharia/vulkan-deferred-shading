//
// Created by johnny on 3/10/26.
//
#pragma once

#include <glm/glm.hpp>

struct MeshPushConstants {
    glm::mat4 modelMatrix; // 64 bytes
    glm::vec4 baseColorFactor; // 16 bytes
};

// Total: 80 bytes (Vulkan minimum guarantee is 128 bytes)