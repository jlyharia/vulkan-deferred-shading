#pragma once
#include "common/VulkanInclude.hpp"
#include <optional>

namespace pass_util {

// =============================================================================
// RenderingAttachmentInfo builders for dynamic rendering (VK_KHR_dynamic_rendering).
// Eliminates the 5-line attachment setup repeated in every pass.
// =============================================================================

[[nodiscard]] inline vk::RenderingAttachmentInfo colorAttachment(
    vk::ImageView                          view,
    vk::AttachmentLoadOp                   loadOp,
    std::optional<vk::ClearColorValue>     clearColor = std::nullopt) noexcept
{
    auto info = vk::RenderingAttachmentInfo()
                .setImageView(view)
                .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setLoadOp(loadOp)
                .setStoreOp(vk::AttachmentStoreOp::eStore);
    if (clearColor)
        info.setClearValue(vk::ClearValue(*clearColor));
    return info;
}

// depthAttachment takes explicit layout and storeOp because passes differ:
//   geometry pass: eDepthStencilAttachmentOptimal, storeOp::eStore
//   overlay pass:  eDepthReadOnlyOptimal,          storeOp::eNone
[[nodiscard]] inline vk::RenderingAttachmentInfo depthAttachment(
    vk::ImageView                               view,
    vk::ImageLayout                             layout,
    vk::AttachmentLoadOp                        loadOp,
    vk::AttachmentStoreOp                       storeOp,
    std::optional<vk::ClearDepthStencilValue>   clearValue = std::nullopt) noexcept
{
    auto info = vk::RenderingAttachmentInfo()
                .setImageView(view)
                .setImageLayout(layout)
                .setLoadOp(loadOp)
                .setStoreOp(storeOp);
    if (clearValue)
        info.setClearValue(vk::ClearValue(*clearValue));
    return info;
}

// =============================================================================
// Viewport/scissor — identical 4-line block in every pass execute().
// =============================================================================

inline void setViewportScissor(vk::CommandBuffer cmd, vk::Extent2D extent) noexcept {
    cmd.setViewport(0, vk::Viewport(0.0f, 0.0f,
                                    static_cast<float>(extent.width),
                                    static_cast<float>(extent.height),
                                    0.0f, 1.0f));
    cmd.setScissor(0, vk::Rect2D({0, 0}, extent));
}

} // namespace pass_util
