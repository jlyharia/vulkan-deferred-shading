//
// Created by johnny on 4/5/26.
//

#include "DirShadowPass.hpp"

#include "PassUtils.hpp"
#include "common/PushConstantConstant.hpp"
#include "scene/DirLightView.hpp"
#include "scene/Mesh.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/ShadowMap.hpp"
#include "vulkan/VulkanUtils.hpp"

DirShadowPass::DirShadowPass(ShadowMap &shadowMap) : shadowMap_(shadowMap) {
}

void DirShadowPass::execute(vk::CommandBuffer cmd,
                            const GraphicsPipeline &pipeline,
                            const std::vector<MeshInstance> &meshInstances,
                            const DirLightView &dirLight) const {
    auto depthAttach = pass_util::depthAttachment(
        shadowMap_.getDepthView(),
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::ClearDepthStencilValue(1.0f, 0));

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, shadowMap_.getExtent()})
                 .setLayerCount(1)
                 .setPDepthAttachment(&depthAttach);

    cmd.beginRendering(renderingInfo);
    {
        pass_util::setViewportScissor(cmd, shadowMap_.getExtent());

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getDirShadowPipeline());

        // Depth bias: pushes shadow map depth slightly away from the surface to prevent self-shadowing.
        // constantFactor shifts all depths uniformly; slopeFactor scales with surface slope.
        // Tune these values if you see shadow acne or Peter Panning.

        cmd.setDepthBias(1.25f, 0.0f, 1.75f);

        const glm::mat4 lightSpace = dirLight.lightSpaceMatrix();

        for (const auto &[mesh, transform, name, color] : meshInstances) {
            if (!mesh)
                continue;

            vk::DeviceSize offsets[] = {0};
            cmd.bindVertexBuffers(0, {mesh->getVertexBuffer()}, offsets);
            cmd.bindIndexBuffer(mesh->getIndexBuffer(), 0, vk::IndexType::eUint32);

            DirShadowDataConstants constants{
                .lightSpaceMatrix = lightSpace,
                .model = transform.modelMatrix
            };
            cmd.pushConstants<DirShadowDataConstants>(
                pipeline.getDirShadowPipelineLayout(),
                vk::ShaderStageFlagBits::eVertex,
                0, constants);

            for (const auto &submesh : mesh->getSubmeshes()) {
                cmd.drawIndexed(submesh.indexCount, 1, submesh.firstIndex, 0, 0);
            }
        }
    }
    cmd.endRendering();
}