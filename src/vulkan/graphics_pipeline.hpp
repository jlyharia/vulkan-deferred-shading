//
// Created by johnny on 12/26/25.
//
#pragma once

#include "common/VulkanInclude.hpp"
#include <vector>

class SwapChain;
class VulkanContext;

class GraphicsPipeline {
public:
    // Constructor & Destructor
    GraphicsPipeline(VulkanContext &context, SwapChain &swapChain,
                     const std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts)
        : context_(context), swapChain_(swapChain) {
        // 1. Create the Layout FIRST
        createPipelineLayout(descriptorSetLayouts);
        // 2. Create the Pipeline SECOND
        createGraphicsPipeline();
    }
    ~GraphicsPipeline();

    // Non-copyable
    GraphicsPipeline(const GraphicsPipeline &) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;

    // Getters
    [[nodiscard]] vk::Pipeline getPipeline() const { return graphicsPipeline_; }
    [[nodiscard]] vk::PipelineLayout getPipelineLayout() const { return pipelineLayout_; }

private:
    // Context references
    VulkanContext &context_;
    SwapChain &swapChain_;

    // Pipeline resources
    vk::PipelineLayout pipelineLayout_;
    vk::Pipeline graphicsPipeline_;

    // for graphic pipeline layout, not descriptor layout
    void createPipelineLayout(const std::vector<vk::DescriptorSetLayout> &desLayouts);
    void createGraphicsPipeline();

    // Helper methods
    [[nodiscard]] vk::ShaderModule createShaderModule(const std::vector<char> &code) const;
};