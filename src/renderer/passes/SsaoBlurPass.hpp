//
// Created by johnny on 4/2/26.
//

#pragma once
#include "common/VulkanInclude.hpp"
#include "vulkan/GraphicsPipeline.hpp"

class SsaoBlurPass {

public:
    SsaoBlurPass(SwapChain &swapChain, VulkanContext &context);
    ~SsaoBlurPass();

    void execute(vk::CommandBuffer cmd,
                 const GraphicsPipeline &pipeline,
                 vk::DescriptorSet blurDescSet) const;

    [[nodiscard]] vk::ImageView getBlurredImageView() const { return ssaoBlurBufferImageView_; }
    [[nodiscard]] vk::Image     getBlurredImage()     const { return ssaoBlurBufferImage_; }

private:
    SwapChain &swapChain_;
    VulkanContext &context_;

    VmaAllocation ssaoBlurBufferAlloc_ = nullptr;
    vk::Image ssaoBlurBufferImage_;
    vk::ImageView ssaoBlurBufferImageView_;
    static constexpr vk::Format SSAO_BLUR_BUFFER_FORMAT = vk::Format::eR8Unorm;

    void createImages(uint32_t width, uint32_t height);
    void cleanup();
};