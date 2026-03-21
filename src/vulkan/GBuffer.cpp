//
// Created by johnny on 3/20/26.
//

#include "GBuffer.hpp"
#include "VulkanContext.hpp"
#include "VulkanUtils.hpp"

GBuffer::GBuffer(VulkanContext &context, uint32_t width, uint32_t height)
    : context_(context) {
    createImages(width, height);
}

GBuffer::~GBuffer() {
    cleanup();
}

void GBuffer::recreate(uint32_t width, uint32_t height) {
    cleanup();
    createImages(width, height);
}

/**
 * @brief Allocates GPU images and creates image views for both G-buffer attachments.
 *
 * Both images use COLOR_ATTACHMENT | SAMPLED usage:
 *  - Written as color attachments during the geometry pass
 *  - Sampled as textures during the lighting pass
 *
 * @param width  Framebuffer width (must match swapchain extent)
 * @param height Framebuffer height (must match swapchain extent)
 */
void GBuffer::createImages(uint32_t width, uint32_t height) {
    constexpr auto usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    auto device = context_.getDevice();
    auto allocator = context_.getVmaAllocator();

    // RT0: Albedo (rgb) + Metallic (a) — 4 bytes/pixel
    vk_util::createImage(allocator, width, height,
                         ALBEDO_METALLIC_FORMAT,
                         vk::ImageTiling::eOptimal,
                         usage,
                         VMA_MEMORY_USAGE_GPU_ONLY,
                         albedoMetallicImage_,
                         albedoMetallicAlloc_);
    albedoMetallicView_ = vk_util::createImageView(device, albedoMetallicImage_,
                                                    ALBEDO_METALLIC_FORMAT);

    // RT1: World Normal (xyz) + Roughness (a) — 8 bytes/pixel
    vk_util::createImage(allocator, width, height,
                         NORMAL_ROUGHNESS_FORMAT,
                         vk::ImageTiling::eOptimal,
                         usage,
                         VMA_MEMORY_USAGE_GPU_ONLY,
                         normalRoughnessImage_,
                         normalRoughnessAlloc_);
    normalRoughnessView_ = vk_util::createImageView(device, normalRoughnessImage_,
                                                     NORMAL_ROUGHNESS_FORMAT);
}

void GBuffer::cleanup() {
    auto device = context_.getDevice();
    auto allocator = context_.getVmaAllocator();

    if (albedoMetallicView_) {
        device.destroyImageView(albedoMetallicView_);
        albedoMetallicView_ = nullptr;
    }
    if (albedoMetallicImage_) {
        vmaDestroyImage(allocator, albedoMetallicImage_, albedoMetallicAlloc_);
        albedoMetallicImage_ = nullptr;
        albedoMetallicAlloc_ = nullptr;
    }

    if (normalRoughnessView_) {
        device.destroyImageView(normalRoughnessView_);
        normalRoughnessView_ = nullptr;
    }
    if (normalRoughnessImage_) {
        vmaDestroyImage(allocator, normalRoughnessImage_, normalRoughnessAlloc_);
        normalRoughnessImage_ = nullptr;
        normalRoughnessAlloc_ = nullptr;
    }
}
