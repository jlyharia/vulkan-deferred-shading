//
// Created by johnny on 3/16/26.
//

#pragma once
#include <glm/glm.hpp>

struct PointLight {
    // xyz = position, w = intensity
    glm::vec4 position;

    // rgb = color, w = radius (or attenuation range)
    glm::vec4 color;
};