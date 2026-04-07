//
// Created by johnny on 4/6/26.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "VulkanUtils.hpp"

class VulkanContext;

/**
 * @brief Owns the depth image used as a directional shadow map.
 *
 * Written exclusively by DirShadowPass (depth attachment, light's POV).
 * Read by the lighting pass as a sampler2D for shadow comparison.
 *
 * Resolution is independent of the swapchain — fixed at construction time.
 * D32Sfloat is guaranteed by the Vulkan spec for eDepthStencilAttachment usage.
 */
class ShadowMap {
public:
    static constexpr vk::Format DEPTH_FORMAT = vk::Format::eD32Sfloat;

    ShadowMap(VulkanContext &context, uint32_t width, uint32_t height);
    ~ShadowMap();

    ShadowMap(const ShadowMap &) = delete;
    ShadowMap &operator=(const ShadowMap &) = delete;

    /// Destroys and re-creates the depth image at a new resolution.
    /// Sampler is resolution-independent and is not recreated.
    void recreate(uint32_t width, uint32_t height);

    [[nodiscard]] vk::ImageView getDepthView()  const { return depth_.view; }
    [[nodiscard]] vk::Image     getDepthImage() const { return depth_.image; }
    [[nodiscard]] vk::Sampler   getSampler()    const { return sampler_; }
    [[nodiscard]] vk::Extent2D  getExtent()     const { return extent_; }

private:
    void createImages(uint32_t width, uint32_t height);
    void createSampler();
    void cleanup();

    VulkanContext &context_;

    vk_util::AttachmentImage depth_;
    vk::Sampler sampler_{};
    vk::Extent2D extent_{};
};
