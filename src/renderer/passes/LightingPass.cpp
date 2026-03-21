#include "LightingPass.hpp"

#include "common/Config.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/SwapChain.hpp"

LightingPass::LightingPass(SwapChain &swapChain) : swapChain_(swapChain) {}

void LightingPass::execute(vk::CommandBuffer cmd,
                           const GraphicsPipeline &pipeline,
                           uint32_t imageIndex,
                           vk::DescriptorSet globalDescSet,
                           vk::DescriptorSet gbufferDescSet) const {
    auto extent = swapChain_.getExtent();
    const vk::PipelineLayout layout = pipeline.getPipelineLayout();

    // eDontCare: fullscreen triangle overwrites every pixel
    auto colorAttachment = vk::RenderingAttachmentInfo()
        .setImageView(swapChain_.getImageViews()[imageIndex])
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, extent})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment);
    // No depth attachment — lighting pass is screen-space only

    cmd.beginRendering(renderingInfo);
    {
        cmd.setViewport(0, vk::Viewport(0.0f, 0.0f,
                                        static_cast<float>(extent.width),
                                        static_cast<float>(extent.height),
                                        0.0f, 1.0f));
        cmd.setScissor(0, vk::Rect2D({0, 0}, extent));

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getLightingPipeline());

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout, DescriptorSets::GLOBAL_SET,
                               {globalDescSet}, {});

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout, DescriptorSets::GBUFFER_SET,
                               {gbufferDescSet}, {});

        cmd.draw(3, 1, 0, 0); // fullscreen triangle — no vertex buffer
    }
    cmd.endRendering();
}
