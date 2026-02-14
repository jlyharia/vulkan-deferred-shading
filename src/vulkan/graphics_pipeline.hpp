//
// Created by johnny on 12/26/25.
//
#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

class SwapChain;
class VulkanContext;

class GraphicsPipeline {
public:
    GraphicsPipeline(VulkanContext& context,
                     SwapChain& swapChain,
                     vk::RenderPass renderPass,
                     vk::DescriptorSetLayout descriptorSetLayout)
        : context_(context), swapChain_(swapChain), renderPass_(renderPass) {

        // 1. Create the Layout FIRST
        createPipelineLayout(descriptorSetLayout);

        // 2. Create the Pipeline SECOND
        createGraphicsPipeline();
    }

    ~GraphicsPipeline();

    // Disable copy
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    [[nodiscard]] vk::Pipeline getPipeline() const { return graphicsPipeline_; }
    [[nodiscard]] vk::PipelineLayout getPipelineLayout() const { return pipelineLayout_; }

private:
    VulkanContext& context_;
    SwapChain& swapChain_;

    // Updated to C++ handles
    vk::PipelineLayout pipelineLayout_;
    vk::RenderPass renderPass_;
    vk::Pipeline graphicsPipeline_;

    void createPipelineLayout(vk::DescriptorSetLayout dsLayout);
    void createGraphicsPipeline();

    // Helper returns the C++ wrapper
    vk::ShaderModule createShaderModule(const std::vector<char>& code) const;
};