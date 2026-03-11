//
// Created by johnny on 3/3/26.
//

#pragma once

#include "common/VulkanInclude.hpp"

struct Texture {
    vk::Image image;
    vk::ImageView imageView;
    VmaAllocation allocation;
    uint32_t width;
    uint32_t height;
    vk::Format format; // CRITICAL: To distinguish sRGB (Color) from Unorm (Normal)
    std::string name;
};