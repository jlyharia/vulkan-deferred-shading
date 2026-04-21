//
// Created by johnny on 4/2/26.
//

#include "SsaoBlurPass.hpp"

#include "PassUtils.hpp"
#include "SsaoPass.hpp"
#include "common/Config.hpp"
#include "vulkan/SwapChain.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanUtils.hpp"


SsaoBlurPass::SsaoBlurPass(SwapChain &swapChain, VulkanContext &context) : swapChain_(swapChain), context_(context) {
    auto ext = swapChain.getExtent();
    createImages(ext.width, ext.height);
}

SsaoBlurPass::~SsaoBlurPass() { cleanup(); }

void SsaoBlurPass::cleanup() {
    ssaoBlurBuffer_.cleanup(context_.getDevice(), context_.getVmaAllocator());
}

void SsaoBlurPass::execute(vk::CommandBuffer cmd,
                           const GraphicsPipeline &pipeline,
                           vk::DescriptorSet blurDescSet) const {
    auto extent = swapChain_.getExtent();
    const vk::PipelineLayout layout = pipeline.getSsaoBlurPipelineLayout();

    auto colorAttachment = pass_util::colorAttachment(ssaoBlurBuffer_.view,
                                                       vk::AttachmentLoadOp::eDontCare);

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, extent})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment);

    cmd.beginRendering(renderingInfo);
    {
        pass_util::setViewportScissor(cmd, extent);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getSsaoBlurPipeline());
        // Blur pipeline has its own layout with a single set (index 0): depth + raw SSAO
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, {blurDescSet}, {});
        cmd.draw(3, 1, 0, 0);
    }
    cmd.endRendering();
}

void SsaoBlurPass::createImages(uint32_t width, uint32_t height) {
    constexpr auto usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    ssaoBlurBuffer_ = vk_util::AttachmentImage::create(context_.getVmaAllocator(), context_.getDevice(),
                                                        width, height, SSAO_BLUR_BUFFER_FORMAT, usage);
}