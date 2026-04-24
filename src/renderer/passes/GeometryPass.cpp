#include "GeometryPass.hpp"

#include "PassUtils.hpp"
#include "common/Config.hpp"
#include "common/PushConstantConstant.hpp"
#include "scene/Mesh.hpp"
#include "vulkan/GBuffer.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/SwapChain.hpp"

GeometryPass::GeometryPass(SwapChain &swapChain, GBuffer &gbuffer)
    : swapChain_(swapChain), gbuffer_(gbuffer) {}

void GeometryPass::execute(vk::CommandBuffer cmd,
                           const GraphicsPipeline &pipeline,
                           vk::DescriptorSet globalDescSet,
                           const std::vector<MeshInstance> &meshInstances,
                           const Material &defaultMaterial,
                           const std::shared_ptr<Mesh> &sphereMesh) const {
    auto extent = swapChain_.getExtent();
    const vk::PipelineLayout layout = pipeline.getPipelineLayout();

    const vk::ClearColorValue zeroClear(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f});
    std::array<vk::RenderingAttachmentInfo, 2> colorAttachments = {
        pass_util::colorAttachment(gbuffer_.getAlbedoMetallicView(),  vk::AttachmentLoadOp::eClear, zeroClear),
        pass_util::colorAttachment(gbuffer_.getNormalRoughnessView(), vk::AttachmentLoadOp::eClear, zeroClear),
    };

    auto depthAttachment = pass_util::depthAttachment(
        swapChain_.getDepthImageView(),
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::ClearDepthStencilValue(0.0f, 0)); // reverse z

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, extent})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachments)
                 .setPDepthAttachment(&depthAttachment);

    cmd.beginRendering(renderingInfo);
    {
        pass_util::setViewportScissor(cmd, extent);

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout, DescriptorSets::GLOBAL_SET,
                               {globalDescSet}, {});

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getGBufferPipeline());

        // Per-mesh draw loop (intentional mirror of ForwardPass — see ForwardPass.cpp).
        // GeometryPass skips unlit materials; ForwardPass handles pipeline switching per-submesh.
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

                if (mat.unlit) continue;

                MeshPushConstants constants;
                constants.modelMatrix     = transform.modelMatrix;
                constants.baseColorFactor = color * mat.baseColorFactor;

                cmd.pushConstants<MeshPushConstants>(
                    layout,
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                    0, constants);

                if (mat.textureSet) {
                    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                           layout, DescriptorSets::MATERIAL_SET,
                                           {mat.textureSet}, {});
                }

                cmd.drawIndexed(submesh.indexCount, 1, submesh.firstIndex, 0, 0);
            }
        }
    }
    cmd.endRendering();
}
