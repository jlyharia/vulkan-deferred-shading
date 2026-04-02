#include "ForwardPass.hpp"

#include "PassUtils.hpp"
#include "common/Config.hpp"
#include "common/PushConstantConstant.hpp"
#include "scene/Mesh.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/SwapChain.hpp"

ForwardPass::ForwardPass(SwapChain &swapChain) : swapChain_(swapChain) {}

void ForwardPass::execute(vk::CommandBuffer cmd,
                          const GraphicsPipeline &pipeline,
                          uint32_t imageIndex,
                          vk::DescriptorSet globalDescSet,
                          const std::vector<MeshInstance> &meshInstances,
                          uint32_t instanceCount,
                          const Material &defaultMaterial,
                          const std::shared_ptr<Mesh> &sphereMesh) const {
    auto colorAttachment = pass_util::colorAttachment(
        swapChain_.getImageViews()[imageIndex],
        vk::AttachmentLoadOp::eClear,
        vk::ClearColorValue(std::array<float, 4>{0.02f, 0.02f, 0.02f, 1.0f}));

    auto depthAttachment = pass_util::depthAttachment(
        swapChain_.getDepthImageView(),
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::ClearDepthStencilValue(1.0f, 0));

    const auto extent = swapChain_.getExtent();
    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, extent})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment)
                 .setPDepthAttachment(&depthAttachment);

    const vk::PipelineLayout layout = pipeline.getPipelineLayout();

    cmd.beginRendering(renderingInfo);
    {
        pass_util::setViewportScissor(cmd, extent);

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout, DescriptorSets::GLOBAL_SET,
                               {globalDescSet}, {});

        vk::Pipeline currentPipeline = nullptr;

        // --- NON-INSTANCED PASS: everything except sphere instances ---
        // Per-mesh draw loop (intentional mirror of GeometryPass — see GeometryPass.cpp).
        // ForwardPass handles pipeline switching per-submesh; GeometryPass skips unlit materials.
        for (const auto &[mesh, transform, name, color] : meshInstances) {
            if (!mesh) continue;
            if (sphereMesh && mesh == sphereMesh) continue;

            vk::DeviceSize offsets[] = {0};
            cmd.bindVertexBuffers(0, {mesh->getVertexBuffer()}, offsets);
            cmd.bindIndexBuffer(mesh->getIndexBuffer(), 0, vk::IndexType::eUint32);

            const auto &materials = mesh->getMaterials();

            for (const auto &submesh : mesh->getSubmeshes()) {
                const auto &mat = (submesh.materialIndex >= 0)
                                      ? materials[submesh.materialIndex]
                                      : defaultMaterial;

                const vk::Pipeline targetPipeline = mat.unlit
                                                        ? pipeline.getUnlitPipeline()
                                                        : pipeline.getPbrPipeline();
                if (targetPipeline != currentPipeline) {
                    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, targetPipeline);
                    currentPipeline = targetPipeline;
                }

                MeshPushConstants constants;
                constants.modelMatrix     = transform.modelMatrix;
                constants.baseColorFactor = color * mat.baseColorFactor;

                cmd.pushConstants<MeshPushConstants>(
                    layout,
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                    0,
                    constants);

                if (mat.textureSet) {
                    cmd.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        layout,
                        DescriptorSets::MATERIAL_SET,
                        {mat.textureSet},
                        {});
                }

                cmd.drawIndexed(submesh.indexCount, 1, submesh.firstIndex, 0, 0);
            }
        }

        // --- INSTANCED PASS: all sphere instances in one draw call ---
        if (sphereMesh && instanceCount > 0) {
            vk::DeviceSize offset = 0;
            cmd.bindVertexBuffers(0, {sphereMesh->getVertexBuffer()}, {offset});
            cmd.bindIndexBuffer(sphereMesh->getIndexBuffer(), 0, vk::IndexType::eUint32);

            const vk::Pipeline unlitPipeline = pipeline.getUnlitPipeline();
            if (unlitPipeline != currentPipeline) {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, unlitPipeline);
            }

            for (const auto &submesh : sphereMesh->getSubmeshes()) {
                cmd.drawIndexed(submesh.indexCount, instanceCount, submesh.firstIndex, 0, 0);
            }
        }
    }
    cmd.endRendering();
}
