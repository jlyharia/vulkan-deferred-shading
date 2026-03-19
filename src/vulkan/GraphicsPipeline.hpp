//
// Created by johnny on 12/26/25.
//
#pragma once

#include "common/VulkanInclude.hpp"
#include <string>
#include <vector>

class SwapChain;
class VulkanContext;

class GraphicsPipeline {
public:
    GraphicsPipeline(VulkanContext &context, SwapChain &swapChain,
                     const std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts)
        : context_(context), swapChain_(swapChain) {
        createPipelineLayout(descriptorSetLayouts);
        pbrPipeline_   = buildPipeline("shaders/pbr/pbr.vert.spv",    "shaders/pbr/pbr.frag.spv");
        unlitPipeline_ = buildPipeline("shaders/unlit/unlit.vert.spv", "shaders/unlit/unlit.frag.spv");
    }
    ~GraphicsPipeline();

    GraphicsPipeline(const GraphicsPipeline &) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;

    [[nodiscard]] vk::Pipeline getPbrPipeline()          const { return pbrPipeline_; }
    [[nodiscard]] vk::Pipeline getUnlitPipeline()        const { return unlitPipeline_; }
    [[nodiscard]] vk::PipelineLayout getPipelineLayout() const { return pipelineLayout_; }

private:
    VulkanContext &context_;
    SwapChain &swapChain_;

    vk::PipelineLayout pipelineLayout_;
    vk::Pipeline pbrPipeline_;
    vk::Pipeline unlitPipeline_;

    void createPipelineLayout(const std::vector<vk::DescriptorSetLayout> &desLayouts);
    [[nodiscard]] vk::Pipeline buildPipeline(const std::string &vertSpv, const std::string &fragSpv) const;
    [[nodiscard]] vk::ShaderModule createShaderModule(const std::vector<char> &code) const;
};
