//
// Created by johnny on 1/21/26.
//

#pragma once
#include <glm/glm.hpp>
#include "scene/PointLight.hpp"

/**
 * This file store Uniform GPU Layout
 */

struct GlobalUBO {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 invView; // inverse view — deferred lighting depth reconstruction
    alignas(16) glm::mat4 invProj; // inverse proj — deferred lighting depth reconstruction
    alignas(16) glm::vec4 cameraPos; // xyz = position, w = unused
    alignas(16) PointLight pointLights[24]; // matches GLSL std140
};

inline constexpr uint32_t SSAO_KERNEL_SIZE = 64;

struct SSAOKernelUBO {
    alignas(16) glm::vec4 samples[SSAO_KERNEL_SIZE];
};
static_assert(sizeof(SSAOKernelUBO) == SSAO_KERNEL_SIZE * sizeof(glm::vec4));