//
// Created by johnny on 12/29/25.
//
#pragma once
#include <vector>
#include <GLFW/glfw3.h>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>
class GraphicsPipeline;
class RenderPass;
class SwapChain;
class VulkanContext;

class Renderer
{
public:
    Renderer(VulkanContext& context,
             SwapChain& swapChain,
             RenderPass& renderPass,
             GLFWwindow* window_);
    ~Renderer();

    // Disable copying: You can't "copy" a GPU renderer
    Renderer(const Renderer&) = delete;

    Renderer& operator=(const Renderer&) = delete;

    void createDescriptorSetLayout();
    void initResources(VkPipelineLayout pipelineLayout);

    void drawFrame(VkPipeline pipeline, bool framebufferResized); // The main function called by App

    void recreateSwapChain(); // call on resize
    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout_; }

private:
    void createCommandPool();

    void createCommandBuffers();

    void createSyncObjects(); // Semaphores and Fences
    void recordCommandBuffer(VkCommandBuffer commandBuffer, VkPipeline pipeline, uint32_t imageIndex) const;

    void createVertexBuffer();
    void createIndexBuffer();
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VmaMemoryUsage vmaUsage,
                      VkBuffer& buffer,
                      VmaAllocation& allocation,
                      VmaAllocationCreateFlags vmaFlags = 0, // Added: for flags like MAPPED
                      VmaAllocationInfo* outAllocInfo = nullptr) const;
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void createAllocator();
    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentFrame) const;
    void createDescriptorPool();
    void createDescriptorSets();

    VulkanContext& context_;
    SwapChain& swapChain_;
    RenderPass& renderPass_;
    //GraphicsPipeline& pipeline_;
    VkPipelineLayout activePipelineLayout_ = VK_NULL_HANDLE;
    GLFWwindow* window_;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    // Sync objects for frames-in-flight
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;

    uint32_t currentFrame = 0;

    std::vector<VkFence> imagesInFlight;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;

    VmaAllocation vertexBufferAllocation_;
    VmaAllocator vmaAllocator;

    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VmaAllocation indexBufferAllocation_;

    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VmaAllocation> uniformBuffersAllocation_;
    std::vector<void*> uniformBuffersMapped_;
    VkDescriptorPool descriptorPool_;
    std::vector<VkDescriptorSet> descriptorSets_;
    VkDescriptorSetLayout descriptorSetLayout_;
};
