//
// Created by johnny on 2/27/26.
//

#pragma once
#include "Transform.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>

class Mesh;

struct MeshInstance {
    std::shared_ptr<Mesh> mesh;
    Transform transform;
    std::string name;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f}; // per-instance tint multiplied with material baseColorFactor
};
