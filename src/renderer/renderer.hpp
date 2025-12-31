//
// Created by johnny on 12/29/25.
//
#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

class GraphicsPipeline;
class RenderPass;
class SwapChain;
class VulkanContext;

class Renderer {
public:
    Renderer(VulkanContext &context, SwapChain &swapChain, RenderPass &renderPass, GraphicsPipeline &pipeline);

    ~Renderer();

    void drawFrame(); // The main function called by App

private:
    void createCommandPool();

    void createCommandBuffers();

    void createSyncObjects(); // Semaphores and Fences
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    VulkanContext &context_;
    SwapChain &swapChain_;
    RenderPass &renderPass_;
    GraphicsPipeline &pipeline_;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    // Sync objects for frames-in-flight
    // std::vector<VkSemaphore> imageAvailableSemaphores_;
    // std::vector<VkSemaphore> renderFinishedSemaphores_;
    // std::vector<VkFence> inFlightFences_;
};
