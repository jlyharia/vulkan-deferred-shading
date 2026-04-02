//
// Created by johnny on 4/2/26.
//

#include "SsaoBlurPass.hpp"

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
    auto device    = context_.getDevice();
    auto allocator = context_.getVmaAllocator();
    if (ssaoBlurBufferImageView_)
        device.destroyImageView(ssaoBlurBufferImageView_);
    if (ssaoBlurBufferImage_)
        vmaDestroyImage(allocator, ssaoBlurBufferImage_, ssaoBlurBufferAlloc_);
}

void SsaoBlurPass::execute(vk::CommandBuffer cmd,
                           const GraphicsPipeline &pipeline,
                           vk::DescriptorSet blurDescSet) const {
    auto extent = swapChain_.getExtent();
    const vk::PipelineLayout layout = pipeline.getSsaoBlurPipelineLayout();

    auto colorAttachment = vk::RenderingAttachmentInfo()
                           .setImageView(ssaoBlurBufferImageView_)
                           .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                           .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                           .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo renderingInfo{};
    renderingInfo.setRenderArea({{0, 0}, extent})
                 .setLayerCount(1)
                 .setColorAttachments(colorAttachment);

    cmd.beginRendering(renderingInfo);
    {
        cmd.setViewport(0, vk::Viewport(0.0f, 0.0f,
                                        static_cast<float>(extent.width),
                                        static_cast<float>(extent.height),
                                        0.0f, 1.0f));
        cmd.setScissor(0, vk::Rect2D({0, 0}, extent));

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getSsaoBlurPipeline());
        // Blur pipeline has its own layout with a single set (index 0): depth + raw SSAO
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, {blurDescSet}, {});
        cmd.draw(3, 1, 0, 0);
    }
    cmd.endRendering();

    // Barrier: blurred output → shader-read for lighting pass
    auto barrier = vk::ImageMemoryBarrier2()
                   .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                   .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                   .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
                   .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
                   .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
                   .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                   .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                   .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                   .setImage(ssaoBlurBufferImage_)
                   .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

    vk::DependencyInfo depInfo;
    depInfo.setImageMemoryBarriers(barrier);
    cmd.pipelineBarrier2(depInfo);
}

void SsaoBlurPass::createImages(uint32_t width, uint32_t height) {
    constexpr auto usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    const auto device = context_.getDevice();
    const auto allocator = context_.getVmaAllocator();

    // RT0: occlusion factor — 1 byte/pixel
    vk_util::createImage(allocator, width, height,
                         SSAO_BLUR_BUFFER_FORMAT,
                         vk::ImageTiling::eOptimal,
                         usage,
                         VMA_MEMORY_USAGE_GPU_ONLY,
                         ssaoBlurBufferImage_,
                         ssaoBlurBufferAlloc_);
    ssaoBlurBufferImageView_ = vk_util::createImageView(device, ssaoBlurBufferImage_, SSAO_BLUR_BUFFER_FORMAT);
}