//
// Created by johnny on 4/6/26.
//

#pragma once
#include <array>
#include "common/VulkanInclude.hpp"
#include "common/Config.hpp"
#include "VulkanUtils.hpp"

class VulkanContext;

/**
 * @brief Owns the 2D array depth image used for cascaded shadow maps.
 *
 * The image has NUM_CASCADES layers (one per cascade), all at the same resolution.
 * DirShadowPass renders into each layer via a per-layer 2D view.
 * The lighting pass samples the full array via a 2DArray view (sampler2DArrayShadow).
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

    /// Full array view (e2DArray, all cascades) — bind to lighting shader as sampler2DArrayShadow.
    [[nodiscard]] vk::ImageView getDepthView()  const { return depthArrayView_; }
    [[nodiscard]] vk::Image     getDepthImage() const { return depthImage_; }
    [[nodiscard]] vk::Sampler   getSampler()    const { return sampler_; }
    [[nodiscard]] vk::Extent2D  getExtent()     const { return extent_; }

    /// Per-layer 2D view for cascade [i] — use as depth attachment in DirShadowPass.
    [[nodiscard]] vk::ImageView getLayerView(int cascade) const { return layerViews_[cascade]; }

private:
    void createImages(uint32_t width, uint32_t height);
    void createSampler();
    void cleanup();

    VulkanContext &context_;

    vk::Image     depthImage_{};
    VmaAllocation depthAlloc_{};
    vk::ImageView depthArrayView_{};  // e2DArray covering all cascades, for sampling
    std::array<vk::ImageView, engineConfig::NUM_CASCADES> layerViews_{};  // one e2D per cascade, for rendering

    vk::Sampler  sampler_{};
    vk::Extent2D extent_{};
};
