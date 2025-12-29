//
// Created by johnny on 12/26/25.
//

#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>


class SwapChain;
class VulkanContext;

class GraphicsPipeline {
public:
    GraphicsPipeline(VulkanContext &context,
                     SwapChain &swapChain,
                     VkRenderPass renderPass)
        : context_(context), swapChain_(swapChain), renderPass_(renderPass) {
        createGraphicsPipeline();
    }

    ~GraphicsPipeline();

private:
    VulkanContext &context_;
    SwapChain &swapChain_;
    VkPipelineLayout pipelineLayout_;
    VkRenderPass renderPass_;
    VkPipeline graphicsPipeline_;


    void createGraphicsPipeline();

    VkShaderModule createShaderModule(const std::vector<char> &code);
};
