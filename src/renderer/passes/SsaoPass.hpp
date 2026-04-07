//
// Created by johnny on 3/28/26.
//

#pragma once

#include "common/Uniform.hpp"
#include "common/VulkanInclude.hpp"
#include "vulkan/GraphicsPipeline.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanUtils.hpp"

#include <glm/glm.hpp>


class SwapChain;

struct SsaoPass {
    explicit SsaoPass(SwapChain &swapChain, VulkanContext &context);
    ~SsaoPass();
    void execute(vk::CommandBuffer cmd, const GraphicsPipeline &pipeline, vk::DescriptorSet globalDescSet,
                 vk::DescriptorSet lightingInputsDescSet, vk::DescriptorSet ssaoDescSet) const;
    [[nodiscard]] vk::ImageView getSsaoKernelBufferImageView() const { return ssaoBuffer_.view; }
    [[nodiscard]] vk::ImageView getSsaoNoiseImageView()        const { return ssaoNoiseImageView_; }
    [[nodiscard]] vk::Buffer    getKernelBuffer()              const { return kernelBuffer_; }
    [[nodiscard]] vk::Image     getSsaoBufferImage()           const { return ssaoBuffer_.image; }
private:
    SwapChain &swapChain_;
    VulkanContext &context_;

    // Noise image: staging-uploaded sampled texture, not a render target — raw members
    vk::Image     ssaoNoiseImage_;
    vk::ImageView ssaoNoiseImageView_;
    VmaAllocation ssaoNoiseAllocation_ = nullptr;

    vk_util::AttachmentImage ssaoBuffer_;  // RT: occlusion factor — 1 byte/pixel


    vk::Buffer kernelBuffer_;
    VmaAllocation kernelBufferAlloc_ = nullptr;


    void generateSsaoNoise();
    void generateSSAOKernel(size_t kernelSize);
    float generateRandomFloat();
    void createImages(uint32_t width, uint32_t height);
    void cleanup();
    // SSAO outputs a single float. Should be vk::Format::eR8Unorm — 1 byte/pixel
    static constexpr vk::Format SSAO_BUFFER_FORMAT = vk::Format::eR8Unorm;
    static constexpr vk::Format SSAO_NOISE_FORMAT = vk::Format::eR16G16Sfloat;


};