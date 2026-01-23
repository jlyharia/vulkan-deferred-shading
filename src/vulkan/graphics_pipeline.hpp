//
// Created by johnny on 12/26/25.
//

#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>


class SwapChain;
class VulkanContext;

class GraphicsPipeline
{
public:
    GraphicsPipeline(VulkanContext& context,
                     SwapChain& swapChain,
                     VkRenderPass renderPass,
                     VkDescriptorSetLayout descriptorSetLayout)
        : context_(context), swapChain_(swapChain), renderPass_(renderPass)
    {
        // 1. Create the Layout FIRST
        createPipelineLayout(descriptorSetLayout);

        // 2. Create the Pipeline SECOND
        createGraphicsPipeline();
    }

    ~GraphicsPipeline();

    [[nodiscard]] VkPipeline getPipeline() const { return graphicsPipeline_; }

    [[nodiscard]] VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }

private:
    VulkanContext& context_;
    SwapChain& swapChain_;
    VkPipelineLayout pipelineLayout_;
    VkRenderPass renderPass_;
    VkPipeline graphicsPipeline_;


    void createPipelineLayout(VkDescriptorSetLayout dsLayout);
    void createGraphicsPipeline();

    VkShaderModule createShaderModule(const std::vector<char>& code);
};
