//
// Created by johnny on 12/27/25.
//

#pragma once
#include <vulkan/vulkan_core.h>


class VulkanContext;
class SwapChain;

class RenderPass {
public:
    RenderPass(VulkanContext &context, VkFormat swapChainImageFormat) : context_(context),
                                                                        swapChainImageFormat_(swapChainImageFormat) {
        createRenderPass();
    }

    ~RenderPass();

    [[nodiscard]] VkRenderPass getRenderPass() const { return renderPass_; }

private:
    VulkanContext &context_;
    VkFormat swapChainImageFormat_;
    VkRenderPass renderPass_;

    void createRenderPass();
};
