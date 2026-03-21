#include "GeometryPass.hpp"

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

    std::array<vk::RenderingAttachmentInfo, 2> colorAttachments;
    colorAttachments[0] = vk::RenderingAttachmentInfo()
        .setImageView(gbuffer_.getAlbedoMetallicView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}));
    colorAttachments[1] = vk::RenderingAttachmentInfo()
        .setImageView(gbuffer_.getNormalRoughnessView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}));

    auto depthAttachment = vk::RenderingAttachmentInfo()
        .setImageView(swapChain_.getDepthImageView())
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, extent})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachments)
                 .setPDepthAttachment(&depthAttachment);

    cmd.beginRendering(renderingInfo);
    {
        cmd.setViewport(0, vk::Viewport(0.0f, 0.0f,
                                        static_cast<float>(extent.width),
                                        static_cast<float>(extent.height),
                                        0.0f, 1.0f));
        cmd.setScissor(0, vk::Rect2D({0, 0}, extent));

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout, DescriptorSets::GLOBAL_SET,
                               {globalDescSet}, {});

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getGBufferPipeline());

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

    // Pipeline barrier: G-buffer color attachments + depth → shader-read for lighting pass
    auto makeColorBarrier = [](vk::Image image) {
        return vk::ImageMemoryBarrier2()
            .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
            .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(image)
            .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    };

    std::array<vk::ImageMemoryBarrier2, 3> barriers = {
        makeColorBarrier(gbuffer_.getAlbedoMetallicImage()),
        makeColorBarrier(gbuffer_.getNormalRoughnessImage()),
        vk::ImageMemoryBarrier2()
            .setSrcStageMask(vk::PipelineStageFlagBits2::eLateFragmentTests)
            .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
            .setOldLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setNewLayout(vk::ImageLayout::eDepthReadOnlyOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(swapChain_.getDepthImage())
            .setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}),
    };

    vk::DependencyInfo depInfo;
    depInfo.setImageMemoryBarriers(barriers);
    cmd.pipelineBarrier2(depInfo);
}
