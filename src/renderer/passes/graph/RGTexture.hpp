#pragma once

#include "common/VulkanInclude.hpp"

struct RGTexture {
    std::string name;
    vk::Image image;
    vk::ImageView view;
    vk::Format format;
    vk::ImageLayout initialLayout; // set on import, never mutated
};
