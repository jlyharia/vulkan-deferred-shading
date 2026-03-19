//
// Created by johnny on 3/16/26.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "common/Vertex.hpp"



namespace vk_mesh {
void generateSphere(float radius,
                    uint32_t sectors,
                    uint32_t stacks,
                    std::vector<Vertex> &vertices,
                    std::vector<uint32_t> &indices);

// Future shapes can go here too!
// void generateCube(...);
}