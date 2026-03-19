//
// Created by johnny on 1/21/26.
//

#pragma once
#include <glm/glm.hpp>
#include "scene/PointLight.hpp"

struct GlobalUBO {
    alignas(16) glm::mat4      view;
    alignas(16) glm::mat4      proj;
    alignas(16) glm::vec4      cameraPos;       // xyz = position, w = unused
    alignas(16) PointLight     pointLights[4];  // offset 144, matches GLSL std140
};