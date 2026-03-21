#pragma once

#include "common/VulkanInclude.hpp"
#include <memory>

class GraphicsPipeline;
class SwapChain;
class Mesh;

struct OverlayPass {
    explicit OverlayPass(SwapChain &swapChain);

    /// Renders light sphere visualization on top of the lit result.
    /// Depth test ON (read-only layout), depth write OFF — preserves the geometry-pass depth.
    void execute(vk::CommandBuffer cmd,
                 const GraphicsPipeline &pipeline,
                 uint32_t imageIndex,
                 vk::DescriptorSet globalDescSet,
                 const std::shared_ptr<Mesh> &sphereMesh,
                 uint32_t instanceCount) const;

private:
    SwapChain &swapChain_;
};
