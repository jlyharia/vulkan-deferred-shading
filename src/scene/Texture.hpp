//
// Created by johnny on 3/3/26.
//

#pragma once

#include "common/VulkanInclude.hpp"

struct Texture {
    // Vulkan Handles
    vk::Image image;
    vk::ImageView imageView;
    vk::Sampler sampler;           // Reference to the manager's sampler
    vk::DescriptorSet descriptorSet; // Pre-baked for Set 1 binding

    // Memory Management
    VmaAllocation allocation;      // CRITICAL for VMA cleanup

    // Metadata
    uint32_t width;
    uint32_t height;
    std::string name;

    // Note: No destructor here! The Manager handles the life cycle.
};