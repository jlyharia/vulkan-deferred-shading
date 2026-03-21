#include "OverlayPass.hpp"

#include "common/Config.hpp"
#include "scene/Mesh.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/SwapChain.hpp"

OverlayPass::OverlayPass(SwapChain &swapChain) : swapChain_(swapChain) {}

void OverlayPass::execute(vk::CommandBuffer cmd,
                          const GraphicsPipeline &pipeline,
                          uint32_t imageIndex,
                          vk::DescriptorSet globalDescSet,
                          const std::shared_ptr<Mesh> &sphereMesh,
                          uint32_t instanceCount) const {
    if (!sphereMesh || instanceCount == 0) return;

    auto extent = swapChain_.getExtent();
    const vk::PipelineLayout layout = pipeline.getPipelineLayout();

    // Load the lit result from the lighting pass
    auto colorAttachment = vk::RenderingAttachmentInfo()
        .setImageView(swapChain_.getImageViews()[imageIndex])
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    // Depth in read-only layout — depth test ON, depth write OFF (via pipeline state)
    auto depthAttachment = vk::RenderingAttachmentInfo()
        .setImageView(swapChain_.getDepthImageView())
        .setImageLayout(vk::ImageLayout::eDepthReadOnlyOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)
        .setStoreOp(vk::AttachmentStoreOp::eNone);

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, extent})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment)
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

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getOverlayUnlitPipeline());

        vk::DeviceSize offset = 0;
        cmd.bindVertexBuffers(0, {sphereMesh->getVertexBuffer()}, {offset});
        cmd.bindIndexBuffer(sphereMesh->getIndexBuffer(), 0, vk::IndexType::eUint32);

        for (const auto &submesh : sphereMesh->getSubmeshes()) {
            cmd.drawIndexed(submesh.indexCount, instanceCount, submesh.firstIndex, 0, 0);
        }
    }
    cmd.endRendering();
}
