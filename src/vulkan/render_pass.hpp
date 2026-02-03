//
// Created by johnny on 12/27/25.
//

#pragma once
#include <vulkan/vulkan_core.h>


class VulkanContext;
class SwapChain;

class RenderPass {
public:
    RenderPass(VulkanContext &context, VkFormat colorFormat, VkFormat depthFormat)
        : context_(context),
          scColorFormat(colorFormat),
          scDepthFormat(depthFormat) {
        createRenderPass();
    }

    ~RenderPass();

    [[nodiscard]] VkRenderPass getRenderPass() const { return renderPass_; }

private:
    VulkanContext &context_;
    VkFormat scColorFormat;
    VkFormat scDepthFormat;
    VkRenderPass renderPass_;

    void createRenderPass();
};