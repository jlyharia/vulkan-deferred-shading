//
// Created by johnny on 12/29/25.
//
#pragma once
#include <vector>
#include <GLFW/glfw3.h>
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
             GraphicsPipeline &pipeline,
             GLFWwindow *window_)
        : context_(context), swapChain_(swapChain), renderPass_(renderPass), pipeline_(pipeline), window_(window_) {
        // 1. The Pool must come first
        createCommandPool();

        // 2. Buffers are allocated FROM the pool
        createCommandBuffers();

        // 3. Sync objects are independent but needed for the first frame
        createSyncObjects();

        createVertexBuffer();

    }

    ~Renderer();

    // Disable copying: You can't "copy" a GPU renderer
    Renderer(const Renderer &) = delete;

    Renderer &operator=(const Renderer &) = delete;

    void drawFrame(bool framebufferResized); // The main function called by App

    void recreateSwapChain(); // call on resize
private:
    void createCommandPool();

    void createCommandBuffers();

    void createSyncObjects(); // Semaphores and Fences
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) const;

    void createVertexBuffer();

    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VulkanContext &context_;
    SwapChain &swapChain_;
    RenderPass &renderPass_;
    GraphicsPipeline &pipeline_;
    GLFWwindow *window_;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    // Sync objects for frames-in-flight
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;

    uint32_t currentFrame = 0;

    std::vector<VkFence> imagesInFlight;
    VkBuffer vertexBuffer_;
    VkDeviceMemory vertexBufferMemory_;

};
