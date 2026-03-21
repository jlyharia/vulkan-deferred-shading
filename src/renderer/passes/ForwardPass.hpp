#pragma once

#include "common/VulkanInclude.hpp"
#include "common/Material.hpp"
#include "scene/MeshInstance.hpp"
#include <memory>
#include <vector>

class GraphicsPipeline;
class SwapChain;
class Mesh;

struct ForwardPass {
    explicit ForwardPass(SwapChain &swapChain);

    /// Full forward pass: renders all geometry (PBR + unlit) and the instanced
    /// light spheres into the swapchain image in a single renderpass.
    void execute(vk::CommandBuffer cmd,
                 const GraphicsPipeline &pipeline,
                 uint32_t imageIndex,
                 vk::DescriptorSet globalDescSet,
                 const std::vector<MeshInstance> &meshInstances,
                 uint32_t instanceCount,
                 const Material &defaultMaterial,
                 const std::shared_ptr<Mesh> &sphereMesh) const;

private:
    SwapChain &swapChain_;
};
