#pragma once

#include "common/VulkanInclude.hpp"
#include "common/Material.hpp"
#include "scene/MeshInstance.hpp"
#include <memory>
#include <vector>

class GraphicsPipeline;
class SwapChain;
class GBuffer;
class Mesh;

struct GeometryPass {
    GeometryPass(SwapChain &swapChain, GBuffer &gbuffer);

    /// Renders all PBR geometry into the G-buffer, then issues the
    /// color-attachment→shader-read barrier so the lighting pass can sample it.
    void execute(vk::CommandBuffer cmd,
                 const GraphicsPipeline &pipeline,
                 vk::DescriptorSet globalDescSet,
                 const std::vector<MeshInstance> &meshInstances,
                 const Material &defaultMaterial,
                 const std::shared_ptr<Mesh> &sphereMesh) const;

private:
    SwapChain &swapChain_;
    GBuffer &gbuffer_;
};
