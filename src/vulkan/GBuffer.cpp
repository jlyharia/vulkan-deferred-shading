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
    auto device    = context_.getDevice();
    auto allocator = context_.getVmaAllocator();

    albedoMetallic_  = vk_util::AttachmentImage::create(allocator, device, width, height,
                                                         ALBEDO_METALLIC_FORMAT, usage);
    normalRoughness_ = vk_util::AttachmentImage::create(allocator, device, width, height,
                                                         NORMAL_ROUGHNESS_FORMAT, usage);
}

void GBuffer::cleanup() {
    albedoMetallic_.cleanup(context_.getDevice(), context_.getVmaAllocator());
    normalRoughness_.cleanup(context_.getDevice(), context_.getVmaAllocator());
}
