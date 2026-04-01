//
// Created by johnny on 12/26/25.
//
#pragma once

#include "common/VulkanInclude.hpp"
#include <string>
#include <vector>

class SwapChain;
class VulkanContext;

/// @brief Pipeline configuration for parameterized pipeline creation.
struct PipelineConfig {
    std::string vertSpv;
    std::string fragSpv;
    std::vector<vk::Format> colorFormats;   // one per MRT attachment
    vk::Format depthFormat = vk::Format::eD32Sfloat;
    bool hasVertexInput = true;             // false for fullscreen triangle
    bool depthTestEnable = true;
    bool depthWriteEnable = true;
    vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack; // eNone for fullscreen passes
};

class GraphicsPipeline {
public:
    GraphicsPipeline(VulkanContext &context, SwapChain &swapChain,
                     const std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts);
    ~GraphicsPipeline();

    GraphicsPipeline(const GraphicsPipeline &) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;

    // Forward rendering pipelines
    [[nodiscard]] vk::Pipeline getPbrPipeline()          const { return pbrPipeline_; }
    [[nodiscard]] vk::Pipeline getUnlitPipeline()        const { return unlitPipeline_; }

    // Deferred rendering pipelines
    [[nodiscard]] vk::Pipeline getGBufferPipeline()      const { return gbufferPipeline_; }
    [[nodiscard]] vk::Pipeline getLightingPipeline()      const { return lightingPipeline_; }
    [[nodiscard]] vk::Pipeline getOverlayUnlitPipeline() const { return overlayUnlitPipeline_; }

    [[nodiscard]] vk::Pipeline getSsaoPipeline() const { return ssaoPipeline_; }
    [[nodiscard]] vk::Pipeline getSsaoBlurPipeline() const { return ssaoBlurPipeline_; }
    [[nodiscard]] vk::PipelineLayout getPipelineLayout() const { return pipelineLayout_; }
    // question. do i need to have pipeline for each shader?
private:
    VulkanContext &context_;
    SwapChain &swapChain_;

    vk::PipelineLayout pipelineLayout_;

    // Forward
    vk::Pipeline pbrPipeline_;
    vk::Pipeline unlitPipeline_;

    // Deferred
    vk::Pipeline gbufferPipeline_;      // geometry pass: 2 color MRT + depth write
    vk::Pipeline lightingPipeline_;     // fullscreen triangle: 1 color, no depth, no vertex input
    vk::Pipeline overlayUnlitPipeline_; // forward overlay: 1 color, depth test ON, depth write OFF
    vk::Pipeline ssaoPipeline_;
    vk::Pipeline ssaoBlurPipeline_;

    void createPipelineLayout(const std::vector<vk::DescriptorSetLayout> &desLayouts);
    [[nodiscard]] vk::Pipeline buildPipeline(const PipelineConfig &config) const;
    [[nodiscard]] vk::ShaderModule createShaderModule(const std::vector<char> &code) const;
};