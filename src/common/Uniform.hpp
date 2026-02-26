//
// Created by johnny on 1/21/26.
//

#pragma once
#include <glm/fwd.hpp>


struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};
