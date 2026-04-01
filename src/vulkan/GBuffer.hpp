//
// Created by johnny on 3/20/26.
//

#pragma once
#include "common/VulkanInclude.hpp"

class VulkanContext;

/**
 * @brief Manages G-buffer render targets for deferred shading.
 *
 * Owns two color attachments used in the geometry pass:
 *  - RT0 (R8G8B8A8_UNORM): albedo.rgb + metallic in alpha
 *  - RT1 (R16G16B16A16_SFLOAT): world-space normal.xyz + roughness in alpha
 *
 * Depth is shared from SwapChain (not owned here).
 * Both images are created with COLOR_ATTACHMENT | SAMPLED usage so they can be
 * written in the geometry pass and read in the lighting pass.
 *
 * Total per-pixel cost: 4 + 8 = 12 bytes (depth adds another 4 from SwapChain).
 * World position is reconstructed from depth in the lighting shader.
 */
class GBuffer {
public:
    GBuffer(VulkanContext &context, uint32_t width, uint32_t height);
    ~GBuffer();

    GBuffer(const GBuffer &) = delete;
    GBuffer &operator=(const GBuffer &) = delete;

    /// Destroys and re-creates images at the new resolution (call on swapchain resize).
    void recreate(uint32_t width, uint32_t height);

    /// @name RT0 — Albedo (rgb) + Metallic (a)
    /// @{
    [[nodiscard]] vk::ImageView getAlbedoMetallicView() const { return albedoMetallicView_; }
    [[nodiscard]] vk::Image getAlbedoMetallicImage() const { return albedoMetallicImage_; }
    /// @}

    /// @name RT1 — World Normal (xyz) + Roughness (a)
    /// @{
    [[nodiscard]] vk::ImageView getNormalRoughnessView() const { return normalRoughnessView_; }
    [[nodiscard]] vk::Image getNormalRoughnessImage() const { return normalRoughnessImage_; }
    /// @}

    /// G-buffer attachment formats (used by pipeline creation and descriptor writes).
    static constexpr vk::Format ALBEDO_METALLIC_FORMAT = vk::Format::eR8G8B8A8Unorm;
    // normal need higher precision than albedo
    static constexpr vk::Format NORMAL_ROUGHNESS_FORMAT = vk::Format::eR16G16B16A16Sfloat;

private:
    void createImages(uint32_t width, uint32_t height);
    void cleanup();

    VulkanContext &context_;

    // RT0
    vk::Image albedoMetallicImage_;
    VmaAllocation albedoMetallicAlloc_ = nullptr;
    vk::ImageView albedoMetallicView_;

    // RT1
    vk::Image normalRoughnessImage_;
    VmaAllocation normalRoughnessAlloc_ = nullptr;
    vk::ImageView normalRoughnessView_;
};
