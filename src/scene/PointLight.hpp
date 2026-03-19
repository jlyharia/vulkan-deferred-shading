//
// Created by johnny on 3/16/26.
//

#pragma once
#include <glm/glm.hpp>

struct PointLight {
    glm::vec4 position; // xyz = world position, w = intensity
    glm::vec4 color;    // xyz = RGB color, w = radius
};