//
// Created by johnny on 3/3/26.
//

#pragma once

#include "common/VulkanInclude.hpp"

struct Texture {
    vk::Image image{};
    vk::ImageView imageView{};
    VmaAllocation allocation{};
    uint32_t width{};
    uint32_t height{};
    uint32_t mipLevels{};
    vk::Format format{}; // CRITICAL: To distinguish sRGB (Color) from Unorm (Linear)
    std::string name;

    // Owned device handles for self-destruction
    vk::Device device{};
    VmaAllocator allocator{};

    Texture() = default;
    ~Texture() {
        if (imageView)
            device.destroyImageView(imageView);
        if (image)
            vmaDestroyImage(allocator, image, allocation);
    }

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;
};