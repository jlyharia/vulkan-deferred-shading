//
// Created by johnny on 12/27/25.
//

#include "render_pass.hpp"

#include "swap_chain.hpp"
#include "VulkanContext.hpp"
#include <array>

RenderPass::~RenderPass() {
    if (renderPass_) {
        context_.getDevice().destroyRenderPass(renderPass_);
    }
}

void RenderPass::createRenderPass() {
    // 1. Color Attachment
    auto colorAttachment = vk::AttachmentDescription()
                           .setFormat(scColorFormat)
                           .setSamples(vk::SampleCountFlagBits::e1)
                           .setLoadOp(vk::AttachmentLoadOp::eClear)
                           .setStoreOp(vk::AttachmentStoreOp::eStore)
                           .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                           .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                           .setInitialLayout(vk::ImageLayout::eUndefined)
                           .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    // 2. Depth Attachment
    auto depthAttachment = vk::AttachmentDescription()
                           .setFormat(scDepthFormat)
                           .setSamples(vk::SampleCountFlagBits::e1)
                           .setLoadOp(vk::AttachmentLoadOp::eClear)
                           .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                           .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                           .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                           .setInitialLayout(vk::ImageLayout::eUndefined)
                           .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    // References
    auto colorAttachmentRef = vk::AttachmentReference()
                              .setAttachment(0)
                              .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    auto depthAttachmentRef = vk::AttachmentReference()
                              .setAttachment(1)
                              .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    // 3. Subpass
    auto subpass = vk::SubpassDescription()
                   .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                   .setColorAttachmentCount(1)
                   .setPColorAttachments(&colorAttachmentRef)
                   .setPDepthStencilAttachment(&depthAttachmentRef);

    // 4. Dependency (Synchronization)
    auto dependency = vk::SubpassDependency()
                      .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                      .setDstSubpass(0)
                      .setSrcStageMask(
                          vk::PipelineStageFlagBits::eColorAttachmentOutput |
                          vk::PipelineStageFlagBits::eLateFragmentTests)
                      .setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
                      .setDstStageMask(
                          vk::PipelineStageFlagBits::eColorAttachmentOutput |
                          vk::PipelineStageFlagBits::eEarlyFragmentTests)
                      .setDstAccessMask(
                          vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    // 5. Create the Render Pass
    std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    auto renderPassInfo = vk::RenderPassCreateInfo()
                          .setAttachments(attachments)
                          .setSubpassCount(1)
                          .setPSubpasses(&subpass)
                          .setDependencyCount(1)
                          .setPDependencies(&dependency);

    renderPass_ = context_.getDevice().createRenderPass(renderPassInfo);
}