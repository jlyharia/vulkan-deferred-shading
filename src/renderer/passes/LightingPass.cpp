#include "LightingPass.hpp"

#include "PassUtils.hpp"
#include "common/Config.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/SwapChain.hpp"

LightingPass::LightingPass(SwapChain &swapChain) : swapChain_(swapChain) {}

void LightingPass::execute(vk::CommandBuffer cmd,
                           const GraphicsPipeline &pipeline,
                           uint32_t imageIndex,
                           vk::DescriptorSet globalDescSet,
                           vk::DescriptorSet lightingInputsDescSet) const {
    auto extent = swapChain_.getExtent();
    const vk::PipelineLayout layout = pipeline.getPipelineLayout();

    // eDontCare: fullscreen triangle overwrites every pixel
    auto colorAttachment = pass_util::colorAttachment(
        swapChain_.getImageViews()[imageIndex], vk::AttachmentLoadOp::eDontCare);

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, extent})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment);
    // No depth attachment — lighting pass is screen-space only

    cmd.beginRendering(renderingInfo);
    {
        pass_util::setViewportScissor(cmd, extent);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getLightingPipeline());

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout, DescriptorSets::GLOBAL_SET,
                               {globalDescSet}, {});

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               layout, DescriptorSets::LIGHTING_INPUTS_SET,
                               {lightingInputsDescSet}, {});

        cmd.draw(3, 1, 0, 0); // fullscreen triangle — no vertex buffer
    }
    cmd.endRendering();
}
