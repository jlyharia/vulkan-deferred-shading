#pragma once

#include "common/VulkanInclude.hpp"

class GraphicsPipeline;
class SwapChain;

struct LightingPass {
    explicit LightingPass(SwapChain &swapChain);

    /// Fullscreen triangle pass: samples G-buffer and evaluates Cook-Torrance BRDF,
    /// writing the lit result to the swapchain image.
    void execute(vk::CommandBuffer cmd,
                 const GraphicsPipeline &pipeline,
                 uint32_t imageIndex,
                 vk::DescriptorSet globalDescSet,
                 vk::DescriptorSet gbufferDescSet) const;

private:
    SwapChain &swapChain_;
};
