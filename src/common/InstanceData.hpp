#pragma once

#include <glm/glm.hpp>

struct InstanceData {
    glm::mat4 modelMatrix; // 64 bytes — columns at offsets 0, 16, 32, 48
    glm::vec4 color;       // 16 bytes — offset 64
};
