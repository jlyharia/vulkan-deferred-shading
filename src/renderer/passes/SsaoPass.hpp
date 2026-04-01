//
// Created by johnny on 3/28/26.
//

#pragma once

#include "common/VulkanInclude.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/VulkanContext.hpp"

#include <glm/glm.hpp>


class SwapChain;

class SsaoPass {
public:
    explicit SsaoPass(SwapChain &swapChain, VulkanContext &context);
    ~SsaoPass();
    void execute(vk::CommandBuffer cmd, const GraphicsPipeline &pipeline, vk::DescriptorSet globalDescSet,
                 vk::DescriptorSet gbufferDescSet, vk::DescriptorSet ssaoDescSet) const;
    [[nodiscard]] vk::ImageView getSsaoKernelBufferImageView() const { return ssaoBufferImageView_; }
    [[nodiscard]] vk::ImageView getSsaoNoiseImageView() const { return ssaoNoiseImageView_; }
    [[nodiscard]] vk::Buffer getKernelBuffer() const { return kernelBuffer_; }
    [[nodiscard]] vk::Image getSsaoBufferImage() const { return ssaoBufferImage_; }
private:
    SwapChain &swapChain_;
    VulkanContext &context_;

    vk::Image ssaoNoiseImage_;
    vk::ImageView ssaoNoiseImageView_;
    VmaAllocation ssaoNoiseAllocation_ = nullptr;

    VmaAllocation ssaoBufferAlloc_ = nullptr;
    vk::Image ssaoBufferImage_;
    vk::ImageView ssaoBufferImageView_;


    vk::Buffer kernelBuffer_;
    VmaAllocation kernelBufferAlloc_ = nullptr;


    void generateSsaoNoise();
    void generateSSAOKernel(size_t kernelSize);
    float generateRandomFloat();
    void createImages(uint32_t width, uint32_t height);
    void cleanup();
    static constexpr size_t SSAO_KERNEL_SIZE = 64;
    // SSAO outputs a single float. Should be vk::Format::eR8Unorm — 1 byte/pixel
    static constexpr vk::Format SSAO_BUFFER_FORMAT = vk::Format::eR8Unorm;
    static constexpr vk::Format SSAO_NOISE_FORMAT = vk::Format::eR32G32B32A32Sfloat;


};