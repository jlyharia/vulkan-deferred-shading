//
// Created by johnny on 12/27/25.
//

#pragma once
#include <vulkan/vulkan.hpp>


class VulkanContext;
class SwapChain;

class RenderPass {
public:
    RenderPass(VulkanContext &context, vk::Format colorFormat, vk::Format depthFormat)
        : context_(context),
          scColorFormat(colorFormat),
          scDepthFormat(depthFormat) {
        createRenderPass();
    }

    ~RenderPass();

    [[nodiscard]] vk::RenderPass getRenderPass() const { return renderPass_; }

private:
    VulkanContext &context_;
    vk::Format scColorFormat;
    vk::Format scDepthFormat;
    vk::RenderPass renderPass_;

    void createRenderPass();
};