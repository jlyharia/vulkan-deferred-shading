//
// Created by johnny on 3/5/26.
//

#pragma once

#include "common/VulkanInclude.hpp"

struct Material {
    std::string name;
    vk::DescriptorSet textureSet; // Set 1: Diffuse Map, etc.

    // Industrial tip: Store the texture objects here too so the Model
    // can destroy them in its destructor.
    // std::shared_ptr<Texture> diffuseTexture;
};