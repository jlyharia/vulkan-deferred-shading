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
    Renderer(VulkanContext &context,
             SwapChain &swapChain,
             RenderPass &renderPass,
             GraphicsPipeline &pipeline)
        : context_(context), swapChain_(swapChain), renderPass_(renderPass), pipeline_(pipeline) {
        // 1. The Pool must come first
        createCommandPool();

        // 2. Buffers are allocated FROM the pool
        createCommandBuffers();

        // 3. Sync objects are independent but needed for the first frame
        createSyncObjects();
    }

    ~Renderer();

    // Disable copying: You can't "copy" a GPU renderer
    Renderer(const Renderer &) = delete;

    Renderer &operator=(const Renderer &) = delete;

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
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;

    uint32_t currentFrame = 0;

    std::vector<VkFence> imagesInFlight;
};
