//
// Created by johnny on 1/21/26.
//

#pragma once
#include <glm/glm.hpp>

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};


struct GlobalUBO {
    alignas(16)glm::mat4 view;
    alignas(16)glm::mat4 proj;
}; // move glm::mat4 model to push constant